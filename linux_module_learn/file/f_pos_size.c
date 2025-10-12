#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/dcache.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <asm/fcntl.h>
#include <asm/processor.h>
#include <asm/uaccess.h>

static struct file *fp = NULL;

static int __init f_pos_size_init(void)
{
	pr_info("&fp->f_pos is %lx\n", (unsigned long)&fp->f_pos);

	return 0;
}

static void __exit f_pos_size_exit(void)
{
}

module_init(f_pos_size_init);
module_exit(f_pos_size_exit);

MODULE_LICENSE("GPL");

// # insmod file2_demo.ko
// [  271.400937] file2_demo: loading out-of-tree module taints kernel.
// [  271.406286] &fp->f_pos is 68
