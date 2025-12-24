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

int __init file_init(void)
{
	int retry = 100;
	int err = 0;
	unsigned char buf1[12] = "hello world.";
	char *file = "/tmp/file_test";

	loff_t pos;

	pr_info("------ file close|open test ------\n");
	fp = filp_open(file, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		err = PTR_ERR(fp);
		pr_err("open file error %ld\n", PTR_ERR(fp));
		goto return__;
	} else {
		pr_info("open file success\n");
	}

	pr_info("fp->f_op: %p, (unsigned long)fp->f_op: 0x%lx\n", fp->f_op,
		(unsigned long)fp->f_op);

	pos = fp->f_pos;
	err = kernel_write(fp, buf1, sizeof(buf1), &pos);
	if (err < 0) {
		pr_err("write file error %d\n", err);
		goto return__;
	}
	fp->f_pos = pos;

	while (retry--) {
		err = filp_close(fp, NULL);
		if (err != 0) {
			pr_err("close file error %d\n", err);
			goto return__;
		} else {
			pr_info("close file success\n");
		}
		pr_info("fp->f_op: %p, (unsigned long)fp->f_op: 0x%lx, retry: %d\n",
			fp->f_op, (unsigned long)fp->f_op, retry);
		ssleep(2);

		fp = filp_open(file, O_RDWR | O_CREAT, 0644);
		if (IS_ERR(fp)) {
			err = PTR_ERR(fp);
			pr_err("open file error %ld\n", PTR_ERR(fp));
			goto return__;
		} else {
			pr_info("open file success\n");
		}
	}
return__:
	return err;
}

void __exit file_exit(void)
{
	if (!IS_ERR_OR_NULL(fp)) {
		pr_info("fp->f_op: %p, (unsigned long)fp->f_op: 0x%lx\n",
			fp->f_op, (unsigned long)fp->f_op);
		filp_close(fp, NULL);
		pr_info("close file success\n");
	}
	pr_info("------ file test exit ------\n");
}

module_init(file_init);
module_exit(file_exit);

MODULE_LICENSE("GPL");


// 测试 open close file 是否受 sig 影响, 结果是没有影响
// # insmod open_file_4.ko
// [156286.662753] ------ file close|open test ------
// [156286.665581] open file success
// [156286.666555] fp->f_op: 000000007cf20131, (unsigned long)fp->f_op: 0xffffffff82215900
// [156286.668786] close file success
// [156286.669733] fp->f_op: 000000007cf20131, (unsigned long)fp->f_op: 0xffffffff82215900, retry: 99
// [156288.685258] open file success
// [156288.686178] close file success
// [156288.686992] fp->f_op: 000000007cf20131, (unsigned long)fp->f_op: 0xffffffff82215900, retry: 98
// [156290.733434] open file success
// [156290.734442] close file success
// [156290.736680] fp->f_op: 000000007cf20131, (unsigned long)fp->f_op: 0xffffffff82215900, retry: 97
// [156292.781482] open file success
// [156292.783761] close file success
// [156292.785720] fp->f_op: 000000007cf20131, (unsigned long)fp->f_op: 0xffffffff82215900, retry: 96
// [156294.829441] open file success
// [156294.831966] close file success
// [156294.834685] fp->f_op: 000000007cf20131, (unsigned long)fp->f_op: 0xffffffff82215900, retry: 95
// [156296.877556] open file success
// [156296.879770] close file success
// [156296.881775] fp->f_op: 000000007cf20131, (unsigned long)fp->f_op: 0xffffffff82215900, retry: 94
// [156298.925504] open file success
// [156298.927835] close file success

// # lsmod
// Module                  Size  Used by    Tainted: G
// open_file_4            24576  1
// fsbackup             1810432  0
// # ps axu|grep insmod
//  1699 root     insmod open_file_4.ko
//  1702 root     grep insmod
// # kill 1699
// # kill 1699
// # ps axu|grep insmod
//  1699 root     insmod open_file_4.ko
//  1704 root     grep insmod
// # ps axu|grep insmod
//  1699 root     insmod open_file_4.ko
//  1706 root     grep insmod
// # kill -9 1699
// # kill -1 1699
// # kill -2 1699
// # kill -3 1699
// # ps axu|grep insmod
//  1709 root     grep insmod
// # lsmod
// Module                  Size  Used by    Tainted: G
// open_file_4            16384  0
// fsbackup             1810432  0

