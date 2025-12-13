#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

static int __init mymodule_init(void)
{
	uint64_t time_ns = ktime_get_ns();
	printk(KERN_INFO "Time in nanoseconds: %llu\n", time_ns);
	return 0;
}

static void __exit mymodule_exit(void)
{
}

module_init(mymodule_init);
module_exit(mymodule_exit);

MODULE_LICENSE("GPL");
