// test_ctr.c – 仅测试 ctr(aes) 加密一个块
#include <linux/module.h>
#include <linux/crypto.h>
#include <crypto/skcipher.h>
#include <linux/scatterlist.h>
#include <linux/random.h>

#define AES_KEY_SIZE 32
#define BLOCK_SIZE 16

static int test_ctr(void) {
    struct crypto_skcipher *tfm;
    struct skcipher_request *req;
    u8 key[AES_KEY_SIZE], iv[16], plain[BLOCK_SIZE], cipher[BLOCK_SIZE];
    struct scatterlist sg;
    int ret;

    tfm = crypto_alloc_skcipher("ctr(aes)", 0, 0);
    if (IS_ERR(tfm)) return PTR_ERR(tfm);

    get_random_bytes(key, AES_KEY_SIZE);
    get_random_bytes(iv, 16);
    get_random_bytes(plain, BLOCK_SIZE);

    ret = crypto_skcipher_setkey(tfm, key, AES_KEY_SIZE);
    if (ret) goto out;

    req = skcipher_request_alloc(tfm, GFP_KERNEL);
    if (!req) { ret = -ENOMEM; goto out; }

    sg_init_one(&sg, cipher, BLOCK_SIZE);
    skcipher_request_set_callback(req, 0, NULL, NULL);
    skcipher_request_set_crypt(req, &sg, &sg, BLOCK_SIZE, iv);

    ret = crypto_skcipher_encrypt(req);
    if (!ret) pr_info("ctr(aes) encryption succeeded\n");

    skcipher_request_free(req);
out:
    crypto_free_skcipher(tfm);
    return ret;
}

static int __init init(void) { return test_ctr(); }
static void __exit exit(void) { pr_info("test_ctr unloaded\n"); }
module_init(init);
module_exit(exit);
MODULE_LICENSE("GPL");
