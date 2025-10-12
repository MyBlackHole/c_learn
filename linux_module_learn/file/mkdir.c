#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <asm/fcntl.h>

static int __init mkdir_init(void)
{
	struct file *file = NULL;

	file = filp_open("/tmp/mkdir_test/", O_DIRECTORY | O_CREAT, S_IRUSR);
	if (IS_ERR(file)) {
		pr_info("Error creating directory\n");
		return -1;
	}

	filp_close(file, NULL);
	pr_info("mkdir_init\n");
	return 0;
}

static void __exit mkdir_exit(void)
{
	pr_info("mkdir_exit\n");
}

module_init(mkdir_init);
module_exit(mkdir_exit);
MODULE_LICENSE("GPL");

// # insmod mkdir.ko
// [   16.473775] Error creating directory
// [   16.492854] Error creating directory
// insmod: can't insert 'mkdir.ko': Operation not permitted
