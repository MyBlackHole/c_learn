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
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>

#define SOCKET_PATH "/tmp/mtls_file_cert_test.sock"
#define CA_CERTS_DIR "/tmp/ca_certs"

typedef struct {
	EVP_PKEY *key;
	X509 *cert;
} CertKeyPair;

typedef struct {
	CertKeyPair ca;
	CertKeyPair server;
	CertKeyPair client;
} CertBundle;

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

void add_ext(X509 *cert, int nid, const char *value)
{
	X509V3_CTX ctx;
	X509V3_set_nconf(&ctx, NULL);
	X509_EXTENSION *ex =
		X509V3_EXT_conf_nid(NULL, &ctx, nid, (char *)value);
	X509_add_ext(cert, ex, -1);
	X509_EXTENSION_free(ex);
}

X509 *generate_ca_cert(EVP_PKEY *pkey, const char *cn)
{
	X509 *cert = X509_new();
	X509_set_version(cert, 2);

	ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);

	X509_NAME *name = X509_NAME_new();
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
				   (const unsigned char *)cn, -1, -1, 0);
	X509_set_subject_name(cert, name);
	X509_set_issuer_name(cert, name);

	X509_set_pubkey(cert, pkey);

	X509_gmtime_adj(X509_get_notBefore(cert), 0);
	X509_gmtime_adj(X509_get_notAfter(cert), 365 * 24 * 3600);

	add_ext(cert, NID_basic_constraints, "critical,CA:TRUE");
	add_ext(cert, NID_key_usage, "critical,keyCertSign,cRLSign");

	X509_sign(cert, pkey, EVP_sha256());

	X509_NAME_free(name);
	return cert;
}

X509 *generate_signed_cert(EVP_PKEY *pkey, X509 *ca_cert, EVP_PKEY *ca_key,
			   const char *cn)
{
	X509 *cert = X509_new();
	X509_set_version(cert, 2);

	ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);

	X509_NAME *name = X509_NAME_new();
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
				   (const unsigned char *)cn, -1, -1, 0);
	X509_set_subject_name(cert, name);
	X509_set_issuer_name(cert, X509_get_subject_name(ca_cert));

	X509_set_pubkey(cert, pkey);

	X509_gmtime_adj(X509_get_notBefore(cert), 0);
	X509_gmtime_adj(X509_get_notAfter(cert), 365 * 24 * 3600);

	X509_sign(cert, ca_key, EVP_sha256());

	X509_NAME_free(name);
	return cert;
}

X509 *load_cert_from_file(const char *filepath)
{
	FILE *fp = fopen(filepath, "r");
	if (!fp)
		return NULL;

	X509 *cert = PEM_read_X509(fp, NULL, NULL, NULL);
	fclose(fp);
	return cert;
}

EVP_PKEY *load_key_from_file(const char *filepath)
{
	FILE *fp = fopen(filepath, "r");
	if (!fp)
		return NULL;

	EVP_PKEY *key = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
	fclose(fp);
	return key;
}

void save_cert_to_file(X509 *cert, const char *filepath)
{
	FILE *fp = fopen(filepath, "w");
	if (!fp)
		return;
	PEM_write_X509(fp, cert);
	fclose(fp);
}

void save_key_to_file(EVP_PKEY *key, const char *filepath)
{
	FILE *fp = fopen(filepath, "w");
	if (!fp)
		return;
	PEM_write_PrivateKey(fp, key, NULL, NULL, 0, NULL, NULL);
	fclose(fp);
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

void print_ca_list_detail(SSL *ssl)
{
	const STACK_OF(X509_NAME) *ca_list = SSL_get0_peer_CA_list(ssl);
	if (!ca_list) {
		printf("    [服务端CA列表] (空)\n");
		return;
	}

	int count = sk_X509_NAME_num(ca_list);
	printf("    [服务端CA列表] 接收数量: %d\n", count);
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

char *get_cert_cn(X509 *cert)
{
	static char buf[256] = { 0 };
	X509_NAME *subject = X509_get_subject_name(cert);
	BIO *bio = BIO_new(BIO_s_mem());
	X509_NAME_print_ex(bio, subject, 0, XN_FLAG_ONELINE);
	BIO_gets(bio, buf, sizeof(buf));
	BIO_free(bio);
	return buf;
}

int match_ca_with_list(X509 *ca_cert, const STACK_OF(X509_NAME) * ca_list)
{
	X509_NAME *ca_subject = X509_get_subject_name(ca_cert);

	for (int i = 0; i < sk_X509_NAME_num(ca_list); i++) {
		X509_NAME *name = sk_X509_NAME_value(ca_list, i);
		if (X509_NAME_cmp(ca_subject, name) == 0) {
			return 1;
		}
	}
	return 0;
}

typedef struct {
	char ca_dir[256];
	EVP_PKEY *client_key;
} CertSelectArg;

int client_cert_file_callback(SSL *ssl, void *arg)
{
	CertSelectArg *csa = (CertSelectArg *)arg;

	printf("\n[证书文件选择回调] ===================\n");

	const STACK_OF(X509_NAME) *ca_list = SSL_get0_peer_CA_list(ssl);
	if (!ca_list) {
		printf("[回调] 未收到服务端信任列表\n");
		return 1;
	}

	print_ca_list_detail(ssl);

	DIR *dir = opendir(csa->ca_dir);
	if (!dir) {
		printf("[回调] 无法打开 CA 目录: %s\n", csa->ca_dir);
		return 0;
	}

	printf("[回调] 扫描 CA 目录: %s\n", csa->ca_dir);

	struct dirent *entry;
	X509 *matched_ca = NULL;
	EVP_PKEY *matched_ca_key = NULL;
	char matched_ca_name[256] = { 0 };

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type != DT_REG)
			continue;

		char filepath[512];
		snprintf(filepath, sizeof(filepath), "%s/%s", csa->ca_dir,
			 entry->d_name);

		char *ext = strrchr(entry->d_name, '.');
		if (!ext || strcmp(ext, ".pem") != 0)
			continue;

		char basepath[600];
		snprintf(basepath, sizeof(basepath), "%s/%s", csa->ca_dir,
			 entry->d_name);

		char keypath[600];
		snprintf(keypath, sizeof(keypath), "%s/%.*s.key", csa->ca_dir,
			 (int)(ext - entry->d_name), entry->d_name);

		X509 *ca_cert = load_cert_from_file(basepath);

		printf("[回调] 检查 CA: %s\n", get_cert_cn(ca_cert));

		if (match_ca_with_list(ca_cert, ca_list)) {
			printf("[回调] 找到匹配的 CA: %s\n",
			       get_cert_cn(ca_cert));

			EVP_PKEY *ca_key = load_key_from_file(keypath);
			if (ca_key) {
				matched_ca = ca_cert;
				matched_ca_key = ca_key;
				strncpy(matched_ca_name, get_cert_cn(ca_cert),
					sizeof(matched_ca_name) - 1);
				break;
			} else {
				printf("[回调] 找到 CA 但无法加载密钥: %s\n",
				       keypath);
				X509_free(ca_cert);
			}
		} else {
			X509_free(ca_cert);
		}
	}
	closedir(dir);

	if (!matched_ca) {
		printf("[回调] 未找到匹配的 CA 证书\n");
		return 0;
	}

	printf("[回调] 使用 CA [%s] 签发客户端证书\n", matched_ca_name);

	EVP_PKEY *client_key = generate_rsa_key(2048);
	X509 *client_cert = generate_signed_cert(
		client_key, matched_ca, matched_ca_key, "ClientFromFile");

	SSL_use_certificate(ssl, client_cert);
	SSL_use_PrivateKey(ssl, client_key);

	printf("[回调] 已设置客户端证书\n");
	print_cert_subject(client_cert, "  客户端证书");

	X509_free(matched_ca);
	EVP_PKEY_free(matched_ca_key);
	EVP_PKEY_free(client_key);
	X509_free(client_cert);

	printf("======================================\n\n");

	return 1;
}

SSL_CTX *create_server_ctx(CertBundle *bundle, const char *ca_dir)
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
	X509_STORE_add_cert(store, bundle->ca.cert);

	DIR *dir = opendir(ca_dir);
	if (dir) {
		struct dirent *entry;
		STACK_OF(X509_NAME) *ca_names = sk_X509_NAME_new_null();
		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_type != DT_REG)
				continue;
			char *ext = strrchr(entry->d_name, '.');
			if (!ext || strcmp(ext, ".pem") != 0)
				continue;
			char filepath[512];
			snprintf(filepath, sizeof(filepath), "%s/%s", ca_dir,
				 entry->d_name);
			X509 *ca_cert = load_cert_from_file(filepath);
			if (ca_cert) {
				X509_NAME *name = X509_NAME_dup(
					X509_get_subject_name(ca_cert));
				sk_X509_NAME_push(ca_names, name);
				X509_STORE_add_cert(store, ca_cert);
				X509_free(ca_cert);
			}
		}
		closedir(dir);
		SSL_CTX_set_client_CA_list(ctx, ca_names);
	}

	SSL_CTX_set_verify(
		ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

	printf("[服务端] SSL_CTX 创建完成\n");
	return ctx;
}

SSL_CTX *create_client_ctx(CertBundle *bundle, CertSelectArg *arg)
{
	printf("[客户端] 创建 SSL_CTX...\n");

	const SSL_METHOD *method = TLS_client_method();
	SSL_CTX *ctx = SSL_CTX_new(method);
	SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
	SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

	X509_STORE *store = SSL_CTX_get_cert_store(ctx);
	X509_STORE_add_cert(store, bundle->ca.cert);

	strncpy(arg->ca_dir, CA_CERTS_DIR, sizeof(arg->ca_dir) - 1);

	SSL_CTX_set_cert_cb(ctx, client_cert_file_callback, arg);

	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

	printf("[客户端] SSL_CTX 创建完成 (证书文件选择回调)\n");
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

	printf("[通信] 服务端 socket: %s\n", path);
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
	printf("[通信] 客户端连接成功\n");
	return sock;
}

void *client_thread(void *arg)
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

void *server_thread(void *arg)
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

void setup_ca_files(CertBundle *bundle)
{
	DIR *dir = opendir(CA_CERTS_DIR);
	if (dir) {
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			char path[512];
			snprintf(path, sizeof(path), "%s/%s", CA_CERTS_DIR,
				 entry->d_name);
			unlink(path);
		}
		closedir(dir);
		rmdir(CA_CERTS_DIR);
	}

	mkdir(CA_CERTS_DIR, 0755);

	char ca1_path[256], ca1_key_path[256];
	snprintf(ca1_path, sizeof(ca1_path), "%s/ca1.pem", CA_CERTS_DIR);
	snprintf(ca1_key_path, sizeof(ca1_key_path), "%s/ca1.key",
		 CA_CERTS_DIR);

	EVP_PKEY *ca1_key = generate_rsa_key(2048);
	X509 *ca1_cert = generate_ca_cert(ca1_key, "FileCA1");
	save_cert_to_file(ca1_cert, ca1_path);
	save_key_to_file(ca1_key, ca1_key_path);
	EVP_PKEY_free(ca1_key);

	char ca2_path[256], ca2_key_path[256];
	snprintf(ca2_path, sizeof(ca2_path), "%s/ca2.pem", CA_CERTS_DIR);
	snprintf(ca2_key_path, sizeof(ca2_key_path), "%s/ca2.key",
		 CA_CERTS_DIR);

	EVP_PKEY *ca2_key = generate_rsa_key(2048);
	X509 *ca2_cert = generate_ca_cert(ca2_key, "FileCA2");
	save_cert_to_file(ca2_cert, ca2_path);
	save_key_to_file(ca2_key, ca2_key_path);
	EVP_PKEY_free(ca2_key);

	printf("[证书] CA 文件已创建:\n");
	printf("  - %s (CN=FileCA1)\n", ca1_path);
	printf("  - %s (CN=FileCA2)\n", ca2_path);

	X509_free(ca1_cert);
	X509_free(ca2_cert);
}

int main()
{
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	printf("=== 客户端证书文件选择测试 (TLS 1.3) ===\n\n");

	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();
	SSL_library_init();
	SSL_load_error_strings();

	setup_ca_files(NULL);

	CertBundle bundle;
	bundle.ca.key = generate_rsa_key(2048);
	bundle.ca.cert = generate_ca_cert(bundle.ca.key, "RootCA");

	bundle.server.key = generate_rsa_key(2048);
	bundle.server.cert = generate_signed_cert(
		bundle.server.key, bundle.ca.cert, bundle.ca.key, "localhost");

	CertSelectArg cert_arg;

	printf("\n[1] 启动服务端...\n");
	SSL_CTX *server_ctx = create_server_ctx(&bundle, CA_CERTS_DIR);
	int server_sock = create_server_socket(SOCKET_PATH);

	printf("\n[2] 启动客户端...\n");
	SSL_CTX *client_ctx = create_client_ctx(&bundle, &cert_arg);
	int client_sock = connect_client_socket(SOCKET_PATH);

	printf("\n[3] 服务端接受连接...\n");
	int client_fd = accept(server_sock, NULL, NULL);
	close(server_sock);

	printf("\n[4] 执行 mTLS 握手...\n");

	SSL *server_ssl = SSL_new(server_ctx);
	SSL *client_ssl = SSL_new(client_ctx);

	void *server_arg[2] = { server_ssl, &client_fd };
	void *client_arg[2] = { client_ssl, &client_sock };

	pthread_t server_t, client_t;
	pthread_create(&server_t, NULL, server_thread, server_arg);
	pthread_create(&client_t, NULL, client_thread, client_arg);

	void *server_result, *client_result;
	pthread_join(server_t, &server_result);
	pthread_join(client_t, &client_result);

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
