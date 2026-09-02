#include <linux/module.h>
#include <linux/crypto.h>
#include <crypto/aead.h>
#include <crypto/skcipher.h>

static int __init trigger_init(void)
{
    struct crypto_skcipher *tfm_sk;
    struct crypto_aead *tfm_aead;

    // 触发 ctr(aes) 实例化
    tfm_sk = crypto_alloc_skcipher("ctr(aes)", 0, 0);
    if (IS_ERR(tfm_sk)) {
        pr_err("Failed to allocate ctr(aes): %ld\n", PTR_ERR(tfm_sk));
    } else {
        pr_info("ctr(aes) instantiated successfully\n");
        crypto_free_skcipher(tfm_sk);
    }

    // 触发 gcm(aes) 实例化
    tfm_aead = crypto_alloc_aead("gcm(aes)", 0, 0);
    if (IS_ERR(tfm_aead)) {
        pr_err("Failed to allocate gcm(aes): %ld\n", PTR_ERR(tfm_aead));
    } else {
        pr_info("gcm(aes) instantiated successfully\n");
        crypto_free_aead(tfm_aead);
    }

    return 0;
}

static void __exit trigger_exit(void)
{
    pr_info("trigger module unloaded\n");
}

module_init(trigger_init);
module_exit(trigger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Trigger ctr(aes) and gcm(aes) instantiation");
