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

int __init acl_test_init(void)
{
	int err = 0;
	unsigned char buf1[12] = "hello world.";
	char *file = "/tmp/file_test";

	loff_t pos;

	pr_info("------ 测试文件无权限写入 ------\n");
	fp = filp_open(file, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		pr_err("open file error %ld\n", PTR_ERR(fp));
		fp = NULL;
		return -1;
	} else {
		pr_info("open file success\n");
	}

	pr_info("fp->f_op: %p, (unsigned long)fp->f_op: 0x%lx\n", fp->f_op,
		(unsigned long)fp->f_op);

	pos = fp->f_pos;
	err = kernel_write(fp, buf1, sizeof(buf1), &pos);
	if (err < 0) {
		pr_err("write file error %d\n", err);
	}
	fp->f_pos = pos;

	return 0;
}

void __exit acl_test_exit(void)
{
	if (!IS_ERR(fp)) {
		pr_info("fp->f_op: %p, (unsigned long)fp->f_op: 0x%lx\n",
			fp->f_op, (unsigned long)fp->f_op);
		filp_close(fp, NULL);
		pr_info("close file success\n");
	}
	pr_info("------ file test exit ------\n");
}

module_init(acl_test_init);
module_exit(acl_test_exit);

MODULE_LICENSE("GPL");


// # touch /tmp/file_test
// # ls -alh /tmp/file_test
// -rw-r--r--    1 root     root           0 Nov  5 01:25 /tmp/file_test
// # chmod 000 /tmp/file_test
// # insmod acl_test.ko
// [55844.352352] acl_test: loading out-of-tree module taints kernel.
// [55844.355419] ------ 测试文件无权限写入 ------
// [55844.357460] open file success
// [55844.358635] fp->f_op: 00000000a30ce22b, (unsigned long)fp->f_op: 0xffffffff82215900
// # ls -alh /tmp/file_test
// ----------    1 root     root          12 Nov  5 01:25 /tmp/file_test
// # cat /tmp/file_test
// hello world.#
