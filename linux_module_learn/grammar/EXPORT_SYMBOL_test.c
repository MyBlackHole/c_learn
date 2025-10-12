#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

extern int global_var;

static int __init export_symbol_test_init(void)
{
	printk("global_var[%d]\n", global_var);
	return 0;
}

static void __exit export_symbol_test_exit(void)
{
	printk("set global_var 0");
	global_var = 0;
	printk("set global_var end\n");
}

module_init(export_symbol_test_init);
module_exit(export_symbol_test_exit);
MODULE_LICENSE("GPL");
