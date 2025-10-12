#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>

static int __init test_hello(void)
{
	printk("init\n");
	if (IS_ERR(NULL)) {
		printk("IS_ERR(NULL) is true\n");
	} else {
		printk("IS_ERR(NULL) is false\n");
	}

	if (IS_ERR_OR_NULL(NULL)) {
		printk("IS_ERR_OR_NULL(NULL) is true\n");
	} else {
		printk("IS_ERR_OR_NULL(NULL) is false\n");
	}
	return 0;
}

static void __exit test_exit(void)
{
	printk("exit\n");
}

module_init(test_hello);
module_exit(test_exit);
MODULE_LICENSE("GPL");
