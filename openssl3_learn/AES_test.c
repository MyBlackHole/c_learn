#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>

void handleErrors()
{
	fprintf(stderr, "An error occurred\n");
	exit(1);
}

void aes_encrypt_decrypt(const unsigned char *input, int input_len,
			 const unsigned char *key, const unsigned char *iv,
			 unsigned char *output, int *output_len, int encrypt)
{
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		handleErrors();

	// 初始化加密或解密
	if (encrypt) {
		EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
	} else {
		EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
	}

	int len = 0;
	if (encrypt) {
		// 加密数据
		EVP_EncryptUpdate(ctx, output, &len, input, input_len);
	} else {
		// 解密数据
		EVP_DecryptUpdate(ctx, output, &len, input, input_len);
	}
	*output_len = len;

	if (encrypt) {
		// 完成加密
		EVP_EncryptFinal_ex(ctx, output + len, &len);
	} else {
		// 完成解密
		EVP_DecryptFinal_ex(ctx, output + len, &len);
	}
	*output_len += len;

	EVP_CIPHER_CTX_free(ctx);
}

int main()
{
	// 256-bit 密钥
	unsigned char key[32] = "01234567890123456789012345678901";
	// 128-bit 初始向量
	unsigned char iv[16] = "0123456789012345";
	unsigned char plaintext[] = "Hello, OpenSSL AES!";
	unsigned char ciphertext[128];
	unsigned char decryptedtext[128];
	int ciphertext_len, decryptedtext_len;

	// 加密
	aes_encrypt_decrypt(plaintext, strlen((char *)plaintext), key, iv,
			    ciphertext, &ciphertext_len, 1);
	printf("Ciphertext: ");
	for (int i = 0; i < ciphertext_len; i++)
		printf("%02x", ciphertext[i]);
	printf("\n");

	// 解密
	aes_encrypt_decrypt(ciphertext, ciphertext_len, key, iv, decryptedtext,
			    &decryptedtext_len, 0);
	decryptedtext[decryptedtext_len] = '\0';
	printf("Decrypted text: %s\n", decryptedtext);

	return 0;
}
