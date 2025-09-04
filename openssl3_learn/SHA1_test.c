#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

int main()
{
	const char *message = "Hello, world!";
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int digest_len;
	EVP_MD_CTX *md_ctx;
	md_ctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(md_ctx, EVP_sha1(), NULL);
	EVP_DigestUpdate(md_ctx, message, strlen(message));
	EVP_DigestFinal_ex(md_ctx, digest, &digest_len);
	EVP_MD_CTX_destroy(md_ctx);
	for (int i = 0; i < digest_len; i++) {
		printf("%02x", digest[i]);
	}
	printf("\n");
	return 0;
}
