#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <stdio.h>

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

void save_key_to_file(EVP_PKEY *pkey, const char *filename, int is_private)
{
	FILE *fp = fopen(filename, "w");
	if (!fp)
		handle_errors();

	if (is_private) {
		if (!PEM_write_PrivateKey(fp, pkey, NULL, NULL, 0, NULL, NULL))
			handle_errors();
	} else {
		if (!PEM_write_PUBKEY(fp, pkey))
			handle_errors();
	}

	fclose(fp);
}

int generate_key()
{
	// 生成 2048 位 RSA 密钥
	EVP_PKEY *pkey = generate_rsa_key(2048);

	// 保存密钥到文件
	save_key_to_file(pkey, "private.pem", 1);
	save_key_to_file(pkey, "public.pem", 0);

	EVP_PKEY_free(pkey);
	EVP_cleanup();
	ERR_free_strings();

	printf("RSA密钥对生成成功！\n");
	return 0;
}

int rsa_encrypt(EVP_PKEY *pub_key, const unsigned char *plaintext,
		int plaintext_len, unsigned char **ciphertext)
{
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pub_key, NULL);
	if (!ctx)
		handle_errors();

	if (EVP_PKEY_encrypt_init(ctx) <= 0)
		handle_errors();

	// 获取输出缓冲区大小
	size_t outlen;
	if (EVP_PKEY_encrypt(ctx, NULL, &outlen, plaintext, plaintext_len) <= 0)
		handle_errors();

	*ciphertext = malloc(outlen);
	if (!*ciphertext)
		handle_errors();

	if (EVP_PKEY_encrypt(ctx, *ciphertext, &outlen, plaintext,
			     plaintext_len) <= 0)
		handle_errors();

	EVP_PKEY_CTX_free(ctx);
	return outlen;
}

int rsa_decrypt(EVP_PKEY *priv_key, const unsigned char *ciphertext,
		int ciphertext_len, unsigned char **plaintext)
{
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(priv_key, NULL);
	if (!ctx)
		handle_errors();

	if (EVP_PKEY_decrypt_init(ctx) <= 0)
		handle_errors();

	// 获取输出缓冲区大小
	size_t outlen;
	if (EVP_PKEY_decrypt(ctx, NULL, &outlen, ciphertext, ciphertext_len) <=
	    0)
		handle_errors();

	*plaintext = malloc(outlen);
	if (!*plaintext)
		handle_errors();

	if (EVP_PKEY_decrypt(ctx, *plaintext, &outlen, ciphertext,
			     ciphertext_len) <= 0)
		handle_errors();

	EVP_PKEY_CTX_free(ctx);
	return outlen;
}

int rsa_encrypt_decrypt()
{
	// 加载公钥和私钥
	FILE *pub_fp = fopen("public.pem", "r");
	FILE *priv_fp = fopen("private.pem", "r");

	EVP_PKEY *pub_key = PEM_read_PUBKEY(pub_fp, NULL, NULL, NULL);
	EVP_PKEY *priv_key = PEM_read_PrivateKey(priv_fp, NULL, NULL, NULL);

	fclose(pub_fp);
	fclose(priv_fp);

	const char *message = "Hello, RSA Encryption!";
	unsigned char *ciphertext = NULL;
	unsigned char *decrypted = NULL;

	// 加密
	int ciphertext_len = rsa_encrypt(pub_key, (unsigned char *)message,
					 strlen(message), &ciphertext);
	printf("加密成功，密文长度: %d\n", ciphertext_len);

	// 解密
	int decrypted_len =
		rsa_decrypt(priv_key, ciphertext, ciphertext_len, &decrypted);
	decrypted[decrypted_len] = '\0';

	printf("解密结果: %s\n", decrypted);

	// 清理
	free(ciphertext);
	free(decrypted);
	EVP_PKEY_free(pub_key);
	EVP_PKEY_free(priv_key);
	EVP_cleanup();
	ERR_free_strings();

	return 0;
}

int rsa_sign(EVP_PKEY *priv_key, const unsigned char *message, int message_len,
	     unsigned char **signature)
{
	EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
	if (!md_ctx)
		handle_errors();

	if (EVP_DigestSignInit(md_ctx, NULL, EVP_sha256(), NULL, priv_key) <= 0)
		handle_errors();

	// 获取签名长度
	size_t siglen;
	if (EVP_DigestSign(md_ctx, NULL, &siglen, message, message_len) <= 0)
		handle_errors();

	*signature = malloc(siglen);
	if (!*signature)
		handle_errors();

	if (EVP_DigestSign(md_ctx, *signature, &siglen, message, message_len) <=
	    0)
		handle_errors();

	EVP_MD_CTX_free(md_ctx);
	return siglen;
}

int rsa_verify(EVP_PKEY *pub_key, const unsigned char *message, int message_len,
	       const unsigned char *signature, int sig_len)
{
	EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
	if (!md_ctx)
		handle_errors();

	if (EVP_DigestVerifyInit(md_ctx, NULL, EVP_sha256(), NULL, pub_key) <=
	    0)
		handle_errors();

	int ret = EVP_DigestVerify(md_ctx, signature, sig_len, message,
				   message_len);

	EVP_MD_CTX_free(md_ctx);
	return ret;
}

int rsa_sign_verify()
{
	// 加载公钥和私钥
	FILE *pub_fp = fopen("public.pem", "r");
	FILE *priv_fp = fopen("private.pem", "r");

	EVP_PKEY *pub_key = PEM_read_PUBKEY(pub_fp, NULL, NULL, NULL);
	EVP_PKEY *priv_key = PEM_read_PrivateKey(priv_fp, NULL, NULL, NULL);

	fclose(pub_fp);
	fclose(priv_fp);

	const char *message = "Hello, RSA Signature!";
	unsigned char *signature = NULL;

	// 签名
	int sig_len = rsa_sign(priv_key, (unsigned char *)message,
			       strlen(message), &signature);
	printf("签名成功，签名长度: %d\n", sig_len);

	// 验证签名
	int verify_result = rsa_verify(pub_key, (unsigned char *)message,
				       strlen(message), signature, sig_len);

	if (verify_result == 1) {
		printf("签名验证成功！\n");
	} else {
		printf("签名验证失败！\n");
	}

	// 清理
	free(signature);
	EVP_PKEY_free(pub_key);
	EVP_PKEY_free(priv_key);
	EVP_cleanup();
	ERR_free_strings();

	return 0;
}

int main()
{
	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();

	generate_key();
	rsa_encrypt_decrypt();
	rsa_sign_verify();
	return 0;
}
