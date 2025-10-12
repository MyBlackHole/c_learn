#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>

// // 不能导出静态变量
// static int global_var = 1;
int global_var = 1;
EXPORT_SYMBOL(global_var);

static int __init export_symbol_init(void)
{
	printk("export_symbol_init\n");
	printk("global_var[%d]\n", global_var);
	return 0;
}

static void __exit export_symbol_exit(void)
{
	printk("export_symbol_exit\n");
	printk("global_var[%d]\n", global_var);
}

module_init(export_symbol_init);
module_exit(export_symbol_exit);
MODULE_LICENSE("GPL");
