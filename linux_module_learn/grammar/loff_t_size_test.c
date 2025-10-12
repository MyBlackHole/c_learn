#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>

static int __init loff_t_size_test_init(void)
{
	pr_info("hello world!\n");
	pr_info("sizeof(loff_t) = %ld\n", sizeof(loff_t));
	return 0;
}

static void __exit loff_t_size_test_exit(void)
{
	pr_info("good bye!\n");
}
module_init(loff_t_size_test_init);
module_exit(loff_t_size_test_exit);
MODULE_LICENSE("GPL");
