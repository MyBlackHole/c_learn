// aes_bare_test.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/crypto.h>
#include <linux/random.h>
#include <crypto/aes.h> // 提供 AES 块大小等常量

#define AES_BLOCK_SIZE 16 // AES 固定块大小 16 字节

static int test_aes_bare(void)
{
	struct crypto_cipher *tfm = NULL;
	u8 key[32]; // AES-256 密钥 32 字节
	u8 plaintext[AES_BLOCK_SIZE];
	u8 ciphertext[AES_BLOCK_SIZE];
	u8 decrypted[AES_BLOCK_SIZE];
	int ret = 0;

	pr_info("=== Bare AES (ECB) Test Start ===\n");

	/* 1. 分配 AES 变换对象 (tfm) */
	tfm = crypto_alloc_cipher("aes", 0, 0);
	if (IS_ERR(tfm)) {
		pr_err("Failed to allocate aes: %ld\n", PTR_ERR(tfm));
		return PTR_ERR(tfm);
	}

	/* 2. 生成随机密钥 (这里演示 AES-256) */
	get_random_bytes(key, 32);
	ret = crypto_cipher_setkey(tfm, key, 32);
	if (ret) {
		pr_err("Set key failed: %d\n", ret);
		goto out;
	}

	/* 3. 准备明文数据 (16 字节) */
	get_random_bytes(plaintext, AES_BLOCK_SIZE);
	pr_info("Plaintext (hex): ");
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET, 16, 1, plaintext,
		       AES_BLOCK_SIZE, true);

	/* 4. 加密：原地加密或拷贝到新缓冲区 */
	memcpy(ciphertext, plaintext, AES_BLOCK_SIZE);
	crypto_cipher_encrypt_one(tfm, ciphertext, ciphertext);
	pr_info("Ciphertext (hex): ");
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET, 16, 1, ciphertext,
		       AES_BLOCK_SIZE, true);

	/* 5. 解密：验证正确性 */
	memcpy(decrypted, ciphertext, AES_BLOCK_SIZE);
	crypto_cipher_decrypt_one(tfm, decrypted, decrypted);
	pr_info("Decrypted (hex): ");
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET, 16, 1, decrypted,
		       AES_BLOCK_SIZE, true);

	/* 6. 验证明文和解密结果是否一致 */
	if (memcmp(plaintext, decrypted, AES_BLOCK_SIZE) == 0) {
		pr_info("✅ AES encrypt/decrypt test PASSED!\n");
	} else {
		pr_info("❌ AES encrypt/decrypt test FAILED!\n");
		ret = -EINVAL;
	}

out:
	crypto_free_cipher(tfm);
	return ret;
}

static int __init aes_test_init(void)
{
	return test_aes_bare();
}

static void __exit aes_test_exit(void)
{
	pr_info("AES bare test module unloaded\n");
}

module_init(aes_test_init);
module_exit(aes_test_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Bare AES (ECB) kernel mode test");
