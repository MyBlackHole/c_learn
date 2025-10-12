#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include "linux/err.h"
// #include <linux/file.h>

static struct file *write_fp = NULL;

static int __init write_init(void)
{
	int64_t write_size = 0;
	char io_buf[1024];
	loff_t pos = 0;
	int i, j;
	// mm_segment_t old_fs;

	// // 6.x 不需要 get_fs
	// old_fs = get_fs();
	// set_fs(KERNEL_DS);
	//
	pr_info("write_init!\n");

	// 缺少目录时会: [ 7175.945832] error: line:29, error code: -2
	write_fp = filp_open("/tmp/write.img", O_RDWR | O_CREAT, 0666);

	if (IS_ERR(write_fp)) {
		pr_err("error code: %ld\n", PTR_ERR(write_fp));
		return -1;
	}

	for (j = 0; j < 1024; j++) {
		io_buf[j] = 'a';
	}

	// 写入 3G 数据
	for (i = 0; i < 3 * 1024 * 1024; i++) {
		// 这里应该有个循环取保写入 1024 字节
		write_size = kernel_write(write_fp, io_buf, 1024, &pos);

		if (write_size <= 0 && write_size != -ERESTARTSYS &&
		    write_size != -EAGAIN && write_size != -EINTR)

		{
			pr_err("error code: %lld\n", write_size);
			break;
		}
		// write_size = vfs_write(write_fp, io_buf, 1024,
		// &pos);
		if (write_size != 1024) {
			pr_err("error code: %lld\n", write_size);
			break;
		}

		// pr_info("write size: %lld\n", write_size);

		// ssleep(10);
	}

	if (filp_close(write_fp, NULL) != 0) {
		pr_err("error code: %ld\n", PTR_ERR(write_fp));
	}

	// set_fs(old_fs);
	return 0;
}
static void __exit write_exit(void)
{
	pr_info("write_exit!\n");
}
module_init(write_init);
module_exit(write_exit);
MODULE_LICENSE("GPL");


// ❯ sudo insmod write.ko
// ❯ ls -alh /tmp/write.img
// Permissions Size User Date Modified Name
// .rw-r--r--  2.1G root 12 10月 19:01  /tmp/write.img
