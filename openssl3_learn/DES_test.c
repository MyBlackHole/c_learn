#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>

#define DES_KEY_SIZE 8
#define DES_IV_SIZE 8

void handleErrors(void)
{
	fprintf(stderr, "An error occurred.\n");
	exit(1);
}

int main(void)
{
	// Initialize OpenSSL
	EVP_CIPHER_CTX *ctx;
	int len;
	int ciphertext_len;
	int plaintext_len;

	unsigned char key[DES_KEY_SIZE] = {
		0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
	}; // 8-byte DES key
	unsigned char iv[DES_IV_SIZE] = { 0x12, 0x34, 0x56, 0x78,
					  0x90, 0xab, 0xcd, 0xef }; // 8-byte IV

	unsigned char *plaintext = (unsigned char *)"This is a secret message.";
	unsigned char ciphertext[128]; // Buffer for ciphertext
	unsigned char decryptedtext[128]; // Buffer for decrypted text

	// --- Encryption ---
	ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		handleErrors();

	// Initialize encryption operation
	if (1 != EVP_EncryptInit_ex(ctx, EVP_des_cbc(), NULL, key, iv))
		handleErrors();

	// Provide the plaintext to be encrypted
	if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext,
				   strlen((char *)plaintext)))
		handleErrors();
	ciphertext_len = len;

	// Finalize the encryption (handle padding)
	if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
		handleErrors();
	ciphertext_len += len;

	EVP_CIPHER_CTX_free(ctx);

	printf("Ciphertext is:\n");
	BIO_dump_fp(stdout, (const char *)ciphertext, ciphertext_len);

	// --- Decryption ---
	ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		handleErrors();

	// Initialize decryption operation
	if (1 != EVP_DecryptInit_ex(ctx, EVP_des_cbc(), NULL, key, iv))
		handleErrors();

	// Provide the ciphertext to be decrypted
	if (1 != EVP_DecryptUpdate(ctx, decryptedtext, &len, ciphertext,
				   ciphertext_len))
		handleErrors();
	plaintext_len = len;

	// Finalize the decryption (handle padding)
	if (1 != EVP_DecryptFinal_ex(ctx, decryptedtext + len, &len))
		handleErrors();
	plaintext_len += len;

	EVP_CIPHER_CTX_free(ctx);

	decryptedtext[plaintext_len] =
		'\0'; // Null-terminate the decrypted text
	printf("Decrypted text is:\n%s\n", decryptedtext);

	return 0;
}
