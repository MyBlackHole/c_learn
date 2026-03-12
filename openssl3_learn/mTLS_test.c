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
#include <sys/wait.h>
#include <pthread.h>

#define SOCKET_PATH "/tmp/mtls_test.sock"

void handle_errors()
{
	ERR_print_errors_fp(stderr);
	abort();
}

void install_msg_callback(SSL_CTX *ctx);
void install_info_callback(SSL_CTX *ctx);
void print_ca_list(SSL *ssl, void *arg);

typedef struct {
	EVP_PKEY *key;
	X509 *cert;
} CertKeyPair;

typedef struct {
	CertKeyPair ca;
	CertKeyPair server;
	CertKeyPair client;
} CertBundle;

void add_ext(X509 *cert, int nid, const char *value)
{
	X509V3_CTX ctx;
	X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
	X509_EXTENSION *ext =
		X509V3_EXT_conf_nid(NULL, &ctx, nid, (char *)value);
	if (ext) {
		X509_add_ext(cert, ext, -1);
		X509_EXTENSION_free(ext);
	}
}

CertKeyPair generate_ca(const char *cn)
{
	CertKeyPair result = { NULL, NULL };

	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
	if (!ctx)
		handle_errors();

	if (EVP_PKEY_keygen_init(ctx) <= 0)
		handle_errors();
	if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0)
		handle_errors();

	if (EVP_PKEY_keygen(ctx, &result.key) <= 0)
		handle_errors();
	EVP_PKEY_CTX_free(ctx);

	result.cert = X509_new();
	if (!result.cert)
		handle_errors();
	X509_set_version(result.cert, 2);

	ASN1_INTEGER_set(X509_get_serialNumber(result.cert), 1);

	X509_NAME *name = X509_NAME_new();
	if (!name)
		handle_errors();
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
				   (const unsigned char *)cn, -1, -1, 0);
	X509_set_subject_name(result.cert, name);
	X509_set_issuer_name(result.cert, name);

	X509_set_pubkey(result.cert, result.key);

	X509_gmtime_adj(X509_get_notBefore(result.cert), 0);
	X509_gmtime_adj(X509_get_notAfter(result.cert), 365 * 24 * 3600);

	add_ext(result.cert, NID_basic_constraints, "critical,CA:TRUE");
	add_ext(result.cert, NID_key_usage, "critical,keyCertSign,cRLSign");

	X509_sign(result.cert, result.key, EVP_sha256());

	X509_NAME_free(name);
	return result;
}

CertKeyPair generate_signed_cert(const char *cn, EVP_PKEY *ca_key,
				 X509 *ca_cert)
{
	CertKeyPair result = { NULL, NULL };

	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
	if (!ctx)
		handle_errors();

	if (EVP_PKEY_keygen_init(ctx) <= 0)
		handle_errors();
	if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0)
		handle_errors();
	if (EVP_PKEY_keygen(ctx, &result.key) <= 0)
		handle_errors();
	EVP_PKEY_CTX_free(ctx);

	result.cert = X509_new();
	if (!result.cert)
		handle_errors();
	X509_set_version(result.cert, 2);

	ASN1_INTEGER_set(X509_get_serialNumber(result.cert), 2);

	X509_NAME *name = X509_NAME_new();
	if (!name)
		handle_errors();
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
				   (const unsigned char *)cn, -1, -1, 0);
	X509_set_subject_name(result.cert, name);
	X509_set_issuer_name(result.cert, X509_get_subject_name(ca_cert));

	X509_set_pubkey(result.cert, result.key);

	X509_gmtime_adj(X509_get_notBefore(result.cert), 0);
	X509_gmtime_adj(X509_get_notAfter(result.cert), 365 * 24 * 3600);

	add_ext(result.cert, NID_basic_constraints, "CA:FALSE");
	add_ext(result.cert, NID_key_usage, "digitalSignature,keyEncipherment");

	X509_sign(result.cert, ca_key, EVP_sha256());

	X509_NAME_free(name);
	return result;
}

CertBundle *generate_all_certs()
{
	CertBundle *bundle = malloc(sizeof(CertBundle));
	memset(bundle, 0, sizeof(CertBundle));

	printf("[证书] 生成 CA 证书...\n");
	bundle->ca = generate_ca("My CA");

	printf("[证书] 生成服务器证书...\n");
	bundle->server = generate_signed_cert("localhost", bundle->ca.key,
					      bundle->ca.cert);

	printf("[证书] 生成客户端证书...\n");
	bundle->client =
		generate_signed_cert("Client", bundle->ca.key, bundle->ca.cert);

	return bundle;
}

void free_cert_bundle(CertBundle *bundle)
{
	EVP_PKEY_free(bundle->ca.key);
	X509_free(bundle->ca.cert);
	EVP_PKEY_free(bundle->server.key);
	X509_free(bundle->server.cert);
	EVP_PKEY_free(bundle->client.key);
	X509_free(bundle->client.cert);
	free(bundle);
}

void save_ca_cert(X509 *ca_cert)
{
	FILE *fp = fopen("ca.pem", "w");
	if (!fp)
		handle_errors();
	PEM_write_X509(fp, ca_cert);
	fclose(fp);
}

void print_cert_info(X509 *cert, const char *name)
{
	printf("  [%s证书]\n", name);

	X509_NAME *subject = X509_get_subject_name(cert);
	char buf[256];
	X509_NAME_oneline(subject, buf, sizeof(buf));
	printf("    Subject: %s\n", buf);

	X509_NAME *issuer = X509_get_issuer_name(cert);
	X509_NAME_oneline(issuer, buf, sizeof(buf));
	printf("    Issuer: %s\n", buf);

	printf("    Serial: %ld\n",
	       ASN1_INTEGER_get(X509_get_serialNumber(cert)));
}

SSL_CTX *create_server_ctx(CertBundle *bundle)
{
	printf("\n[服务端] 创建 SSL_CTX...\n");

	const SSL_METHOD *method = TLS_server_method();
	SSL_CTX *ctx = SSL_CTX_new(method);

	if (!ctx)
		handle_errors();

	SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
	SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

	SSL_CTX_set_security_level(ctx, 0);

	if (SSL_CTX_use_certificate(ctx, bundle->server.cert) <= 0) {
		handle_errors();
	}

	if (SSL_CTX_use_PrivateKey(ctx, bundle->server.key) <= 0) {
		handle_errors();
	}

	if (!SSL_CTX_check_private_key(ctx)) {
		handle_errors();
	}

	X509_STORE *store = SSL_CTX_get_cert_store(ctx);
	X509_STORE_add_cert(store, bundle->ca.cert);

	STACK_OF(X509_NAME) *ca_names = sk_X509_NAME_new_null();
	X509_NAME *name = X509_NAME_dup(X509_get_subject_name(bundle->ca.cert));
	sk_X509_NAME_push(ca_names, name);
	SSL_CTX_set_client_CA_list(ctx, ca_names);

	SSL_CTX_set_verify(
		ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

	install_msg_callback(ctx);
	install_info_callback(ctx);

	printf("[服务端] SSL_CTX 创建完成\n");
	return ctx;
}

SSL_CTX *create_client_ctx(CertBundle *bundle)
{
	printf("\n[客户端] 创建 SSL_CTX...\n");

	const SSL_METHOD *method = TLS_client_method();
	SSL_CTX *ctx = SSL_CTX_new(method);

	if (!ctx)
		handle_errors();

	SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
	SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

	SSL_CTX_set_security_level(ctx, 0);

	if (SSL_CTX_use_certificate(ctx, bundle->client.cert) <= 0) {
		handle_errors();
	}

	if (SSL_CTX_use_PrivateKey(ctx, bundle->client.key) <= 0) {
		handle_errors();
	}

	if (!SSL_CTX_check_private_key(ctx)) {
		handle_errors();
	}

	X509_STORE *store = X509_STORE_new();
	X509_STORE_add_cert(store, bundle->ca.cert);
	SSL_CTX_set_cert_store(ctx, store);

	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

	install_msg_callback(ctx);
	install_info_callback(ctx);

	printf("[客户端] SSL_CTX 创建完成\n");
	return ctx;
}

int create_server_socket(const char *path)
{
	unlink(path);

	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0)
		handle_errors();

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		handle_errors();
	}

	if (listen(sockfd, 1) < 0) {
		handle_errors();
	}

	printf("[通信] 服务端 socket 创建成功: %s\n", path);
	return sockfd;
}

int connect_client_socket(const char *path)
{
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0)
		handle_errors();

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		handle_errors();
	}

	printf("[通信] 客户端连接成功: %s\n", path);
	return sockfd;
}

void cleanup_socket(const char *path)
{
	unlink(path);
}

void print_ssl_info(SSL *ssl, const char *side)
{
	printf("\n[%s] SSL 连接信息:\n", side);

	const char *version = SSL_get_version(ssl);
	printf("  TLS Version: %s\n", version);

	const char *cipher = SSL_get_cipher(ssl);
	printf("  Cipher Suite: %s\n", cipher);

	const SSL_CIPHER *c = SSL_get_current_cipher(ssl);
	if (c) {
		printf("  Protocol: %s\n", SSL_CIPHER_get_version(c));
		printf("  Bits: %d\n", SSL_CIPHER_get_bits(c, NULL));
	}

	X509 *peer_cert = SSL_get_peer_certificate(ssl);
	if (peer_cert) {
		printf("  Peer Certificate:\n");
		X509_NAME *subject = X509_get_subject_name(peer_cert);
		char buf[256];
		X509_NAME_oneline(subject, buf, sizeof(buf));
		printf("    Subject: %s\n", buf);
		X509_free(peer_cert);
	}

	long verify_result = SSL_get_verify_result(ssl);
	printf("  Verify Result: %s\n",
	       X509_verify_cert_error_string(verify_result));
}

void print_trusted_ca_list(SSL *ssl)
{
	printf("  信任的 CA 列表:\n");

	SSL_CTX *ctx = SSL_get_SSL_CTX(ssl);
	X509_STORE *store = SSL_CTX_get_cert_store(ctx);

	char buf[256];
	STACK_OF(X509_OBJECT) *objs = X509_STORE_get0_objects(store);
	int count = 0;

	for (int i = 0; i < sk_X509_OBJECT_num(objs); i++) {
		X509_OBJECT *obj = sk_X509_OBJECT_value(objs, i);
		if (X509_OBJECT_get_type(obj) == X509_LU_X509) {
			X509 *cert = X509_OBJECT_get0_X509(obj);
			X509_NAME *subject = X509_get_subject_name(cert);
			X509_NAME_oneline(subject, buf, sizeof(buf));
			printf("    [%d] %s\n", ++count, buf);
		}
	}

	if (count == 0) {
		printf("    (无)\n");
	}
}

const char *ssl_msg_type(int type)
{
	switch (type) {
	case 20:
		return "ChangeCipherSpec";
	case 21:
		return "Alert";
	case 22:
		return "Handshake";
	case 23:
		return "ApplicationData";
	case 25:
		return "Heartbeat";
	default:
		return "Unknown";
	}
}

const char *handshake_msg_type(int type)
{
	switch (type) {
	case 0:
		return "ClientHello";
	case 1:
		return "ServerHello";
	case 2:
		return "HelloVerifyRequest";
	case 11:
		return "Certificate";
	case 12:
		return "ServerKeyExchange";
	case 13:
		return "CertificateRequest";
	case 14:
		return "ServerHelloDone";
	case 15:
		return "CertificateVerify";
	case 16:
		return "ClientKeyExchange";
	case 20:
		return "Finished";
	default:
		return "Unknown";
	}
}

void print_ca_list(SSL *ssl, void *arg)
{
	const char *role = (const char *)arg;
	int is_server = strcmp(role, "server") == 0;

	const STACK_OF(X509_NAME) * ca_list;
	if (is_server)
		ca_list = SSL_CTX_get_client_CA_list(SSL_get_SSL_CTX(ssl));
	else
		ca_list = SSL_get0_peer_CA_list(ssl);

	if (!ca_list) {
		printf("      (CA list empty or not available)\n");
		return;
	}

	int count = sk_X509_NAME_num(ca_list);
	printf("      信任 CA 数量: %d\n", count);
	for (int i = 0; i < count; i++) {
		X509_NAME *name = sk_X509_NAME_value(ca_list, i);
		BIO *bio = BIO_new(BIO_s_mem());
		X509_NAME_print_ex(bio, name, 0, XN_FLAG_ONELINE);
		char buf[1024] = { 0 };
		BIO_gets(bio, buf, sizeof(buf));
		printf("        └─ CA[%d]: %s\n", i + 1, buf);
		BIO_free(bio);
	}
}

void ssl_msg_callback(int write_p, int version, int content_type,
		      const void *buf, size_t len, SSL *ssl, void *arg)
{
	if (content_type != 22)
		return;

	const unsigned char *p = (const unsigned char *)buf;
	int hs_type = p[0];

	const char *role = (const char *)arg;
	const char *action;

	switch (hs_type) {
	case 0:
		action = write_p ? "发送 ClientHello" : "接收 ClientHello";
		printf("  [握手] [%-6s] %s (TLS %d.%d)\n", role, action,
		       (version >> 8) & 0xFF, version & 0xFF);
		break;
	case 1: {
		action = write_p ? "发送 ServerHello" : "接收 ServerHello";
		const char *cipher = SSL_get_cipher(ssl);
		printf("  [握手] [%-6s] %s\n", role, action);
		if (write_p)
			printf("        └─ 加密套件: %s\n",
			       cipher ? cipher : "(未确定)");
		break;
	}
	case 2:
		action = write_p ? "发送 HelloRetryRequest (重协商)" :
				   "接收 HelloRetryRequest";
		printf("  [握手] [%-6s] %s\n", role, action);
		break;
	case 8:
		action = write_p ? "发送加密扩展 (EncryptedExtensions)" :
				   "接收加密扩展";
		printf("  [握手] [%-6s] %s\n", role, action);
		break;
	case 13:
		action = write_p ? "发送信任列表 (CertificateRequest)" :
				   "接收信任列表";
		printf("  [握手] [%-6s] %s\n", role, action);
		if (write_p)
			print_ca_list(ssl, arg);
		break;
	case 11: {
		int is_server_role = strcmp(role, "server") == 0;
		X509 *cert = NULL;
		if (write_p) {
			cert = SSL_get_certificate(ssl);
			printf("  [握手] [%-6s] 发送%s证书\n", role,
			       is_server_role ? "服务器" : "客户端");
		} else {
			cert = SSL_get_peer_certificate(ssl);
			printf("  [握手] [%-6s] 接收%s证书\n", role,
			       is_server_role ? "客户端" : "服务器");
		}
		if (cert) {
			X509_NAME *subject = X509_get_subject_name(cert);
			X509_NAME *issuer = X509_get_issuer_name(cert);
			BIO *bio = BIO_new(BIO_s_mem());
			char buf2[1024] = { 0 };
			X509_NAME_print_ex(bio, subject, 0, XN_FLAG_ONELINE);
			BIO_gets(bio, buf2, sizeof(buf2));
			printf("        └─ Subject: %s\n", buf2);
			X509_NAME_print_ex(bio, issuer, 0, XN_FLAG_ONELINE);
			BIO_gets(bio, buf2, sizeof(buf2));
			printf("        └─ Issuer:  %s\n", buf2);
			BIO_free(bio);
		}
		break;
	}
	case 15:
		action = write_p ? "发送 CertificateVerify" :
				   "接收 CertificateVerify";
		printf("  [握手] [%-6s] %s\n", role, action);
		printf("        └─ 用私钥签名，证明拥有私钥\n");
		break;
	case 20:
		action = write_p ? "发送 Finished" : "接收 Finished";
		printf("  [握手] [%-6s] %s\n", role, action);
		printf("        └─ 验证握手消息完整性\n");
		break;
	default:
		break;
	}
}

void install_msg_callback(SSL_CTX *ctx)
{
	SSL_CTX_set_msg_callback(ctx, ssl_msg_callback);
}

void ssl_info_callback(const SSL *ssl, int where, int ret)
{
	const char *role = SSL_is_server(ssl) ? "Client" : "Server";
	int w = where;

	if (w & SSL_CB_HANDSHAKE_START) {
		printf("[%s] ▼ 握手开始\n", role);
	} else if (w & SSL_CB_HANDSHAKE_DONE) {
		printf("[%s] ▲ 握手完成\n", role);
	}
}

void install_info_callback(SSL_CTX *ctx)
{
	SSL_CTX_set_info_callback(ctx, ssl_info_callback);
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
	print_ssl_info(client_ssl, "Client");
	print_trusted_ca_list(client_ssl);
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
	print_ssl_info(server_ssl, "Server");
	print_trusted_ca_list(server_ssl);
	return (void *)1;
}

int main()
{
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	printf("=== mTLS 握手案例 (TLS 1.3) ===\n\n");

	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();
	SSL_library_init();
	SSL_load_error_strings();

	printf("[1] 生成证书...\n");
	CertBundle *bundle = generate_all_certs();
	save_ca_cert(bundle->ca.cert);

	print_cert_info(bundle->ca.cert, "CA");
	print_cert_info(bundle->server.cert, "Server");
	print_cert_info(bundle->client.cert, "Client");

	printf("\n[2] 启动服务端...\n");
	SSL_CTX *server_ctx = create_server_ctx(bundle);
	int server_sock = create_server_socket(SOCKET_PATH);

	printf("\n[3] 启动客户端...\n");
	SSL_CTX *client_ctx = create_client_ctx(bundle);
	int client_sock = connect_client_socket(SOCKET_PATH);

	printf("\n[4] 服务端接受连接...\n");
	int client_fd = accept(server_sock, NULL, NULL);
	if (client_fd < 0)
		handle_errors();
	close(server_sock);

	printf("\n[5] 执行 mTLS 握手 (使用线程)...\n");

	SSL *server_ssl = SSL_new(server_ctx);
	SSL *client_ssl = SSL_new(client_ctx);

	SSL_set_msg_callback_arg(server_ssl, (void *)"server");
	SSL_set_msg_callback_arg(client_ssl, (void *)"client");

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

	printf("\n[6] 验证双向认证...\n");
	X509 *server_cert = SSL_get_peer_certificate(server_ssl);
	X509 *client_cert = SSL_get_peer_certificate(client_ssl);

	if (server_cert && client_cert) {
		printf("  [OK] 服务器证书验证通过\n");
		printf("  [OK] 客户端证书验证通过\n");
		printf("  [OK] mTLS 双向认证成功!\n");
		X509_free(server_cert);
		X509_free(client_cert);
	}

	printf("\n[7] 测试加密通信...\n");
	const char *msg = "Hello from mTLS!";
	unsigned char buf[256];

	int n = SSL_write(client_ssl, msg, strlen(msg));
	printf("  [Client] 发送: %s (%d bytes)\n", msg, n);

	n = SSL_read(server_ssl, buf, sizeof(buf));
	buf[n] = '\0';
	printf("  [Server] 接收: %s (%d bytes)\n", buf, n);

	const char *reply = "Hello from Server!";
	n = SSL_write(server_ssl, reply, strlen(reply));
	printf("  [Server] 发送: %s (%d bytes)\n", reply, n);

	n = SSL_read(client_ssl, buf, sizeof(buf));
	buf[n] = '\0';
	printf("  [Client] 接收: %s (%d bytes)\n", buf, n);

	printf("\n[8] 清理资源...\n");
	SSL_shutdown(server_ssl);
	SSL_shutdown(client_ssl);
	SSL_free(server_ssl);
	SSL_free(client_ssl);
	close(client_fd);
	close(client_sock);
	cleanup_socket(SOCKET_PATH);

	SSL_CTX_free(server_ctx);
	SSL_CTX_free(client_ctx);
	free_cert_bundle(bundle);

	EVP_cleanup();
	ERR_free_strings();

	printf("\n=== mTLS 握手案例完成 ===\n");
	return 0;
}
