#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

int main()
{
	EVP_MD_CTX *mdctx;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;

	mdctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);
	EVP_DigestUpdate(mdctx, "Hello", 5);
	EVP_DigestFinal_ex(mdctx, md_value, &md_len);
	EVP_MD_CTX_destroy(mdctx);
	printf("MD5 of \"Hello\" is: ");
	for (int i = 0; i < md_len; i++) {
		printf("%02x", md_value[i]);
	}
	printf("\n");
	return 0;
}
