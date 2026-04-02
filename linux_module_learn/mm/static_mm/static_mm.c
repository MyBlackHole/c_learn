#include <linux/init.h>
#include <linux/module.h>

// static int size = 128; // 默认大小
// module_param(size, int, 0644);
// MODULE_PARM_DESC(size, "Size of dynamic array");

#define SIZE 1 * 1024 * 1024 * 100

static int dynamic_array[SIZE];

int __init my_init(void)
{
	int i = 0;
	pr_info("open demo init\n");
	for (; i < SIZE; i++) {
		dynamic_array[i] = 'a';
	}
	pr_info("%.*s\n", 100, (char *)dynamic_array);
	return 0;
}

void __exit my_exit(void)
{
	pr_info("open demo exit\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");

// # insmod static_mm.ko
// [   24.448164] static_mm: loading out-of-tree module taints kernel.
// [   24.842041] open demo init
// # lsmod
// Module                  Size  Used by    Tainted: G
// static_mm           419446784  0
