#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>

static int __init while_init(void)
{
	pr_info("hello world!\n");
	return 0;
}

static void __exit while_exit(void)
{
	int index = 0;
	while (1) {
		ssleep(10);
		pr_info("%d sys hook count: %d", __LINE__, index);
		index++;
		if (index > 10)
			break;
	}
	pr_info("good bye!\n");
}
module_init(while_init);
module_exit(while_exit);
MODULE_LICENSE("GPL");
