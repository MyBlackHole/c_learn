#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#define SOCKET_PATH "/tmp/mtls_multi_cert_test.sock"

typedef struct {
	EVP_PKEY *key;
	X509 *cert;
} CertKeyPair;

typedef struct {
	CertKeyPair ca1;
	CertKeyPair ca2;
	CertKeyPair server;
	CertKeyPair client1;
	CertKeyPair client2;
} MultiCertBundle;

typedef struct {
	X509 *cert;
	EVP_PKEY *key;
	const char *ca_cn;
} ClientCertInfo;

void handle_errors()
{
	ERR_print_errors_fp(stderr);
	abort();
}

EVP_PKEY *generate_rsa_key(int bits)
{
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
	if (!ctx)
		handle_errors();

	if (EVP_PKEY_keygen_init(ctx) <= 0)
		handle_errors();
	if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) <= 0)
		handle_errors();

	EVP_PKEY *pkey = NULL;
	if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
		handle_errors();
	EVP_PKEY_CTX_free(ctx);

	return pkey;
}

X509 *generate_self_signed_cert(EVP_PKEY *pkey, const char *cn)
{
	X509 *cert = X509_new();
	X509_NAME *name = X509_NAME_new();

	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
				   (const unsigned char *)cn, -1, -1, 0);
	X509_set_subject_name(cert, name);
	X509_set_issuer_name(cert, name);

	X509_gmtime_adj(X509_get_notBefore(cert), 0);
	X509_gmtime_adj(X509_get_notAfter(cert), 365 * 24 * 3600);

	X509_set_pubkey(cert, pkey);

	X509_sign(cert, pkey, EVP_sha256());
	return cert;
}

X509 *generate_signed_cert(EVP_PKEY *pkey, X509 *ca_cert, EVP_PKEY *ca_key,
			   const char *cn)
{
	X509 *cert = X509_new();
	X509_NAME *name = X509_NAME_new();

	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
				   (const unsigned char *)cn, -1, -1, 0);
	X509_set_subject_name(cert, name);
	X509_set_issuer_name(cert, X509_get_subject_name(ca_cert));

	X509_gmtime_adj(X509_get_notBefore(cert), 0);
	X509_gmtime_adj(X509_get_notAfter(cert), 365 * 24 * 3600);

	X509_set_pubkey(cert, pkey);

	X509_sign(cert, ca_key, EVP_sha256());
	return cert;
}

void print_ca_list_detail(SSL *ssl)
{
	const STACK_OF(X509_NAME) *ca_list = SSL_get0_peer_CA_list(ssl);
	if (!ca_list) {
		printf("    [CA列表] (空)\n");
		return;
	}

	int count = sk_X509_NAME_num(ca_list);
	printf("    [CA列表] 接收到的信任 CA 数量: %d\n", count);
	for (int i = 0; i < count; i++) {
		X509_NAME *name = sk_X509_NAME_value(ca_list, i);
		BIO *bio = BIO_new(BIO_s_mem());
		X509_NAME_print_ex(bio, name, 0, XN_FLAG_ONELINE);
		char buf[1024] = { 0 };
		BIO_gets(bio, buf, sizeof(buf));
		printf("      [%d] %s\n", i + 1, buf);
		BIO_free(bio);
	}
}

void print_cert_subject(X509 *cert, const char *label)
{
	if (!cert) {
		printf("    %s: (无)\n", label);
		return;
	}
	X509_NAME *subject = X509_get_subject_name(cert);
	BIO *bio = BIO_new(BIO_s_mem());
	X509_NAME_print_ex(bio, subject, 0, XN_FLAG_ONELINE);
	char buf[1024] = { 0 };
	BIO_gets(bio, buf, sizeof(buf));
	printf("    %s: %s\n", label, buf);
	BIO_free(bio);
}

int client_cert_select_callback(SSL *ssl, void *arg)
{
	printf("\n[客户端证书选择回调] ==========\n");

	const STACK_OF(X509_NAME) *ca_list = SSL_get0_peer_CA_list(ssl);
	if (!ca_list) {
		printf("[回调] 未收到服务端信任列表，使用默认证书\n");
		return 1;
	}

	print_ca_list_detail(ssl);

	ClientCertInfo *certs = (ClientCertInfo *)arg;
	int match_count = 0;

	for (int i = 0; i < 2; i++) {
		X509 *cert = certs[i].cert;
		if (!cert)
			continue;

		X509_NAME *cert_issuer = X509_get_issuer_name(cert);

		for (int j = 0; j < sk_X509_NAME_num(ca_list); j++) {
			X509_NAME *ca_name = sk_X509_NAME_value(ca_list, j);

			if (X509_NAME_cmp(cert_issuer, ca_name) == 0) {
				printf("[回调] 找到匹配证书!\n");
				print_cert_subject(cert, "  匹配的客户端证书");

				SSL_use_certificate(ssl, cert);
				SSL_use_PrivateKey(ssl, certs[i].key);

				printf("[回调] 已选择证书: %s\n",
				       certs[i].ca_cn);
				match_count++;
				break;
			}
		}
	}

	if (match_count == 0) {
		printf("[回调] 未找到匹配的证书，使用第一个\n");
		SSL_use_certificate(ssl, certs[0].cert);
		SSL_use_PrivateKey(ssl, certs[0].key);
	}

	printf("==================================\n\n");

	return 1;
}

SSL_CTX *create_server_ctx(MultiCertBundle *bundle)
{
	printf("[服务端] 创建 SSL_CTX...\n");

	const SSL_METHOD *method = TLS_server_method();
	SSL_CTX *ctx = SSL_CTX_new(method);
	SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
	SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

	if (SSL_CTX_use_certificate(ctx, bundle->server.cert) <= 0)
		handle_errors();
	if (SSL_CTX_use_PrivateKey(ctx, bundle->server.key) <= 0)
		handle_errors();
	if (!SSL_CTX_check_private_key(ctx))
		handle_errors();

	X509_STORE *store = SSL_CTX_get_cert_store(ctx);
	X509_STORE_add_cert(store, bundle->ca1.cert);
	X509_STORE_add_cert(store, bundle->ca2.cert);

	STACK_OF(X509_NAME) *ca_names = sk_X509_NAME_new_null();
	X509_NAME *name1 =
		X509_NAME_dup(X509_get_subject_name(bundle->ca1.cert));
	X509_NAME *name2 =
		X509_NAME_dup(X509_get_subject_name(bundle->ca2.cert));
	sk_X509_NAME_push(ca_names, name1);
	sk_X509_NAME_push(ca_names, name2);
	SSL_CTX_set_client_CA_list(ctx, ca_names);

	SSL_CTX_set_verify(
		ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

	printf("[服务端] SSL_CTX 创建完成\n");
	return ctx;
}

SSL_CTX *create_client_ctx(MultiCertBundle *bundle, ClientCertInfo *certs)
{
	printf("[客户端] 创建 SSL_CTX...\n");

	const SSL_METHOD *method = TLS_client_method();
	SSL_CTX *ctx = SSL_CTX_new(method);
	SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
	SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

	X509_STORE *store = SSL_CTX_get_cert_store(ctx);
	X509_STORE_add_cert(store, bundle->ca1.cert);
	X509_STORE_add_cert(store, bundle->ca2.cert);

	certs[0].cert = bundle->client1.cert;
	certs[0].key = bundle->client1.key;
	certs[0].ca_cn = "CA1";

	certs[1].cert = bundle->client2.cert;
	certs[1].key = bundle->client2.key;
	certs[1].ca_cn = "CA2";

	SSL_CTX_set_cert_cb(ctx, client_cert_select_callback, certs);

	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

	printf("[客户端] SSL_CTX 创建完成 (设置了证书选择回调)\n");
	return ctx;
}

int create_server_socket(const char *path)
{
	unlink(path);

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	listen(sock, 1);

	printf("[通信] 服务端 socket 创建成功: %s\n", path);
	return sock;
}

int connect_client_socket(const char *path)
{
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	connect(sock, (struct sockaddr *)&addr, sizeof(addr));

	printf("[通信] 客户端连接成功: %s\n", path);
	return sock;
}

void *client_handshake_thread(void *arg)
{
	SSL *client_ssl = (SSL *)((void **)arg)[0];
	int client_sock = *(int *)((void **)arg)[1];

	printf("[Client] 开始 TLS 握手...\n");
	SSL_set_fd(client_ssl, client_sock);

	int ret;
	while ((ret = SSL_connect(client_ssl)) != 1) {
		int err = SSL_get_error(client_ssl, ret);
		if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
			usleep(10000);
			continue;
		}
		printf("[Client] SSL_connect 失败: %d\n", err);
		ERR_print_errors_fp(stderr);
		return NULL;
	}

	printf("[Client] TLS 握手成功!\n");
	X509 *peer = SSL_get_peer_certificate(client_ssl);
	print_cert_subject(peer, "  对端证书");

	return (void *)1;
}

void *server_handshake_thread(void *arg)
{
	SSL *server_ssl = (SSL *)((void **)arg)[0];
	int client_fd = *(int *)((void **)arg)[1];

	printf("[Server] 开始 TLS 握手...\n");
	SSL_set_fd(server_ssl, client_fd);

	int ret;
	while ((ret = SSL_accept(server_ssl)) != 1) {
		int err = SSL_get_error(server_ssl, ret);
		if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
			usleep(10000);
			continue;
		}
		printf("[Server] SSL_accept 失败: %d\n", err);
		ERR_print_errors_fp(stderr);
		return NULL;
	}

	printf("[Server] TLS 握手成功!\n");
	X509 *peer = SSL_get_peer_certificate(server_ssl);
	print_cert_subject(peer, "  对端证书");

	return (void *)1;
}

MultiCertBundle *generate_all_certs()
{
	printf("[证书] 生成测试证书...\n");

	MultiCertBundle *bundle = calloc(1, sizeof(MultiCertBundle));

	bundle->ca1.key = generate_rsa_key(2048);
	bundle->ca1.cert = generate_self_signed_cert(bundle->ca1.key, "CA1");

	bundle->ca2.key = generate_rsa_key(2048);
	bundle->ca2.cert = generate_self_signed_cert(bundle->ca2.key, "CA2");

	bundle->server.key = generate_rsa_key(2048);
	bundle->server.cert =
		generate_signed_cert(bundle->server.key, bundle->ca1.cert,
				     bundle->ca1.key, "localhost");

	bundle->client1.key = generate_rsa_key(2048);
	bundle->client1.cert = generate_signed_cert(bundle->client1.key,
						    bundle->ca1.cert,
						    bundle->ca1.key, "Client1");

	bundle->client2.key = generate_rsa_key(2048);
	bundle->client2.cert = generate_signed_cert(bundle->client2.key,
						    bundle->ca2.cert,
						    bundle->ca2.key, "Client2");

	printf("[证书] CA1: /CN=CA1\n");
	printf("[证书] CA2: /CN=CA2\n");
	printf("[证书] Server: /CN=localhost (由 CA1 签发)\n");
	printf("[证书] Client1: /CN=Client1 (由 CA1 签发)\n");
	printf("[证书] Client2: /CN=Client2 (由 CA2 签发)\n");

	return bundle;
}

int main()
{
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	printf("=== 多证书客户端选择测试 (TLS 1.3) ===\n\n");

	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();
	SSL_library_init();
	SSL_load_error_strings();

	MultiCertBundle *bundle = generate_all_certs();

	ClientCertInfo certs[2];

	printf("\n[1] 启动服务端...\n");
	SSL_CTX *server_ctx = create_server_ctx(bundle);
	int server_sock = create_server_socket(SOCKET_PATH);

	printf("\n[2] 启动客户端...\n");
	SSL_CTX *client_ctx = create_client_ctx(bundle, certs);
	int client_sock = connect_client_socket(SOCKET_PATH);

	printf("\n[3] 服务端接受连接...\n");
	int client_fd = accept(server_sock, NULL, NULL);
	close(server_sock);

	printf("\n[4] 执行 mTLS 握手...\n");

	SSL *server_ssl = SSL_new(server_ctx);
	SSL *client_ssl = SSL_new(client_ctx);

	void *server_arg[2] = { server_ssl, &client_fd };
	void *client_arg[2] = { client_ssl, &client_sock };

	pthread_t server_thread, client_thread;
	pthread_create(&server_thread, NULL, server_handshake_thread,
		       server_arg);
	pthread_create(&client_thread, NULL, client_handshake_thread,
		       client_arg);

	void *server_result, *client_result;
	pthread_join(server_thread, &server_result);
	pthread_join(client_thread, &client_result);

	if (!server_result || !client_result) {
		printf("  [ERROR] 握手失败\n");
		return 1;
	}

	printf("\n[5] 验证结果...\n");
	X509 *server_cert = SSL_get_peer_certificate(server_ssl);
	X509 *client_cert = SSL_get_peer_certificate(client_ssl);

	if (server_cert && client_cert) {
		printf("  [OK] 双向认证成功\n");
		print_cert_subject(server_cert, "  服务器证书");
		print_cert_subject(client_cert, "  客户端证书");
	}

	SSL_free(server_ssl);
	SSL_free(client_ssl);
	SSL_CTX_free(server_ctx);
	SSL_CTX_free(client_ctx);
	close(client_fd);
	close(client_sock);
	unlink(SOCKET_PATH);

	printf("\n=== 测试完成 ===\n");
	return 0;
}
