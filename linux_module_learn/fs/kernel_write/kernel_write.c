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

int __init my_init(void)
{
	int err = 0;
	unsigned char buf1[12] = "hello world.";
	int count = 1000;

	loff_t pos;

	pr_info("open demo enter\n");
	fp = filp_open("/tmp/file_test", O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		printk("create file error\n");
		return -1;
	}
	pr_info("&fp->f_op is %lx\n", (unsigned long)&fp->f_op);
	pr_info("fp->f_op is %lx\n", (unsigned long)fp->f_op);
	pr_info("fp->f_op phy addr is 0x%lx\n",
		(unsigned long)virt_to_phys((void *)(unsigned long)&fp->f_op));
	pr_info("fp->f_count is %ld\n", fp->f_count.counter);

	while (count--) {
		pos = fp->f_pos;
		err = kernel_write(fp, buf1, sizeof(buf1), &pos);
		if (err < 0) {
			pr_err("write file error %d\n", err);
			break;
		}
		if (err == 0) {
			pr_info("write file end\n");
			break;
		}
		fp->f_pos += err;
		pr_info("write file %d bytes, pos is %lld\n", err, fp->f_pos);
		msleep(1000);
	}

	return 0;
}

void __exit my_exit(void)
{
	pr_info("open demo exit\n");
	pr_info("fp->f_op is %lx\n", (unsigned long)&fp->f_op);
	pr_info("fp->f_count is %ld\n", fp->f_count.counter);
	pr_info("fp->f_op phy addr is 0x%lx\n",
		(unsigned long)virt_to_phys((void *)(unsigned long)&fp->f_op));
	if (fp) {
		filp_close(fp, NULL);
	}
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");

// # insmod kernel_write.ko
// [87933.579971] open demo enter
// [87933.582814] &fp->f_op is ffff888230219f28
// [87933.586781] fp->f_op is ffffffff82215900
// [87933.590697] fp->f_op phy addr is 0x230219f28
// [87933.594586] fp->f_count is 1
// [87933.597402] write file 12 bytes, pos is 12
// [87934.637465] write file 12 bytes, pos is 24
// [87935.661420] write file 12 bytes, pos is 36
// [87936.685472] write file 12 bytes, pos is 48
// [87937.709496] write file 12 bytes, pos is 60
// [87938.733449] write file 12 bytes, pos is 72
// [87939.757426] write file 12 bytes, pos is 84
// [87940.781412] write file 12 bytes, pos is 96
// [87941.805431] write file 12 bytes, pos is 108
// [87942.829660] write file 12 bytes, pos is 120
// [87943.853226] write file 12 bytes, pos is 132
// [87944.877495] write file 12 bytes, pos is 144
// [87945.901410] write file 12 bytes, pos is 156
// [87946.925374] write file 12 bytes, pos is 168
// [87947.949393] write file 12 bytes, pos is 180
// [87948.973464] write file 12 bytes, pos is 192
// [87949.997361] write file 12 bytes, pos is 204
// [87951.021468] write file 12 bytes, pos is 216
// [87952.045306] write file 12 bytes, pos is 228
// [87953.069416] write file 12 bytes, pos is 240
// [87954.093504] write file 12 bytes, pos is 252
// [87955.117438] write file 12 bytes, pos is 264
// [87956.141450] write file error -4
// Terminated


// # ps axu|grep insm
//  1643 root     insmod kernel_write.ko
//  1646 root     grep insm
// # kill 1643
