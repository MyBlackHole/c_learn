#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched/signal.h>
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
	bool pending = 0;

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
		// if (signal_pending(current))
		// 	return -EINTR;
		if (signal_pending(current)) {
			pending = 1;
			pr_info("clear signal pending\n");
			clear_thread_flag(TIF_SIGPENDING);
		}

		pos = fp->f_pos;
		err = kernel_write(fp, buf1, sizeof(buf1), &pos);
		if (err < 0) {
			if (err == -EAGAIN || err == -EBUSY || err == -EINTR) {
				pr_info("write file error %d, retry\n", err);
				msleep(1000);
				continue;
			}
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

	if (pending) {
		pr_info("set signal pending\n");
		set_thread_flag(TIF_SIGPENDING);
		return -EINTR;
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

// 重拾写入, 不清除信号, 永远不会写入成功
// [89623.021397] write file error -4, retry
// [89624.045479] write file error -4, retry
// [89625.069439] write file error -4, retry
// [89626.093443] write file error -4, retry
// ...
// ...
// ...
// [89628.141463] write file error -4, retry
// [89629.165464] write file error -4, retry
// [89630.189446] write file error -4, retry
// Terminated

// # ps axu|grep insm
//  1843 root     insmod kernel_write_4.ko
//  1846 root     grep insm
// # kill 1843

// # lsmod
// Module                  Size  Used by    Tainted: G
// kernel_write_4         16384  0

// 测试识别是否存在信号堵塞, insmod 不会成功, 有概率出现 -4
// # insmod kernel_write_4.ko
// [90143.309805] open demo enter
// [90143.311737] &fp->f_op is ffff88822f0fb728
// [90143.313972] fp->f_op is ffffffff82215900
// [90143.316714] fp->f_op phy addr is 0x22f0fb728
// [90143.319628] fp->f_count is 1
// [90143.321725] write file 12 bytes, pos is 12
// [90144.365458] write file 12 bytes, pos is 24
// [90145.389524] write file 12 bytes, pos is 36
// [90146.413456] write file 12 bytes, pos is 48
// [90147.437382] write file 12 bytes, pos is 60
// [90148.461327] write file 12 bytes, pos is 72
// [90149.485474] write file 12 bytes, pos is 84
// [90150.509318] write file 12 bytes, pos is 96
// [90151.533459] write file 12 bytes, pos is 108
// [90152.557495] write file 12 bytes, pos is 120
// [90153.581331] write file 12 bytes, pos is 132
// [90154.605390] write file 12 bytes, pos is 144
// [90155.629444] write file 12 bytes, pos is 156
// [90156.653452] write file 12 bytes, pos is 168
// [90157.677426] write file 12 bytes, pos is 180
// [90158.701455] write file 12 bytes, pos is 192
// [90159.725453] write file 12 bytes, pos is 204
// [90160.749317] write file 12 bytes, pos is 216
// [90161.773505] write file 12 bytes, pos is 228
// [90162.797289] write file 12 bytes, pos is 240
// [90163.821536] write file 12 bytes, pos is 252
// [90164.845471] write file 12 bytes, pos is 264
// [90165.869526] write file 12 bytes, pos is 276
// [90166.893420] write file 12 bytes, pos is 288
// Terminated

// # lsmod
// Module                  Size  Used by    Tainted: G
// kernel_write_4         24576  1
// fsbackup             1810432  0
// # ps axu|grep insm
//  1664 root     insmod kernel_write_4.ko
//  1667 root     grep insm
// # kill 1664
// # lsmod
// Module                  Size  Used by    Tainted: G
// fsbackup             1810432  0


// 拦截信号与恢复信号测试写入
// # insmod kernel_write_4.ko
// [94497.854607] open demo enter
// [94497.857336] &fp->f_op is ffff8882300da128
// [94497.861279] fp->f_op is ffffffff82215900
// [94497.864819] fp->f_op phy addr is 0x2300da128
// [94497.867669] fp->f_count is 1
// [94497.869552] write file 12 bytes, pos is 12
// [94498.925519] write file 12 bytes, pos is 24
// [94499.949423] write file 12 bytes, pos is 36
// [94500.973525] write file 12 bytes, pos is 48
// [94501.997406] write file 12 bytes, pos is 60
// [94503.021436] write file 12 bytes, pos is 72
// [94504.045427] write file 12 bytes, pos is 84
// [94505.069594] write file 12 bytes, pos is 96
// [94506.093508] write file 12 bytes, pos is 108
// [94507.117436] write file 12 bytes, pos is 120
// [94508.141501] write file 12 bytes, pos is 132
// [94509.165341] write file 12 bytes, pos is 144
// [94510.189361] write file 12 bytes, pos is 156
// [94511.213306] write file 12 bytes, pos is 168
// [94512.237461] write file 12 bytes, pos is 180
// [94513.261329] write file 12 bytes, pos is 192
// [94514.285455] write file 12 bytes, pos is 204
// [94515.309526] write file 12 bytes, pos is 216
// [94516.333555] write file 12 bytes, pos is 228
// [94517.357470] write file 12 bytes, pos is 240
// [94518.381430] write file 12 bytes, pos is 252
// [94519.405465] write file 12 bytes, pos is 264
// [94520.429450] write file 12 bytes, pos is 276
// [94521.453436] write file 12 bytes, pos is 288
// [94522.477406] write file 12 bytes, pos is 300
// [94523.501617] write file 12 bytes, pos is 312
// [94524.525465] write file 12 bytes, pos is 324
// [94525.549380] write file 12 bytes, pos is 336
// [94526.573354] clear signal pending
// [94526.575868] write file 12 bytes, pos is 348
// [94527.597422] write file 12 bytes, pos is 360
// [94528.621356] write file 12 bytes, pos is 372
// [94529.645414] write file 12 bytes, pos is 384
// [94530.669423] write file 12 bytes, pos is 396
// [94531.693387] write file 12 bytes, pos is 408
// [94532.717314] write file 12 bytes, pos is 420
// [94533.741379] write file 12 bytes, pos is 432
// [94534.765539] write file 12 bytes, pos is 444
// [94535.789355] write file 12 bytes, pos is 456
// [94536.813411] clear signal pending
// [94536.816359] write file 12 bytes, pos is 468
// [94537.837490] write file 12 bytes, pos is 480
// [94538.861336] write file 12 bytes, pos is 492
// [94539.885567] write file 12 bytes, pos is 504
// [94540.909472] clear signal pending
// [94540.911487] write file 12 bytes, pos is 516
// [94541.933449] write file 12 bytes, pos is 528
// [94542.957529] write file 12 bytes, pos is 540
// [94543.981420] write file 12 bytes, pos is 552
// [94545.005582] clear signal pending
// [94545.007499] write file 12 bytes, pos is 564
// [94546.029432] write file 12 bytes, pos is 576
// [94547.053444] write file 12 bytes, pos is 588
// [94548.077291] write file 12 bytes, pos is 600
// [94549.101437] write file 12 bytes, pos is 612
// [94550.125485] write file 12 bytes, pos is 624



// # lsmod
// Module                  Size  Used by    Tainted: G
// kernel_write_4         24576  1
// fsbackup             1810432  0
// # ps axu|grep insm
//  1681 root     insmod kernel_write_4.ko
//  1685 root     grep insm
// # kill 1681
// # kill 1681
// # kill 1681
// # kill 1681
// # kill -9 1681
// # kill -8 1681
// # kill -1 1681

