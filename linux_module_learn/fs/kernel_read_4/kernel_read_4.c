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
	unsigned char buf1[12] = "";
	int count = 100;
	bool pending = 0;

	loff_t pos = 0;

	pr_info("open demo enter\n");
	fp = filp_open("/root/file_test", O_RDWR | O_CREAT, 0644);
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

		err = kernel_read(fp, buf1, 10, &pos);
		if (err < 0) {
			if (err == -EAGAIN || err == -EBUSY || err == -EINTR) {
				pr_info("read file error %d, retry\n", err);
				msleep(1000);
				continue;
			}
			pr_err("read file error %d\n", err);
			break;
		}
		if (err == 0) {
			pr_info("read file end\n");
			break;
		}
		pr_info("read file %d bytes, pos is %s\n", err, buf1);
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
	if (!IS_ERR_OR_NULL(fp)) {
		filp_close(fp, NULL);
	}
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");

// 测试 kernel_read 是否受信号影响, 结论是会有影响
// # insmod kernel_read_4.ko
// [157488.978599] open demo enter
// [157488.979539] &fp->f_op is ffff8882306e2828
// [157488.980781] fp->f_op is ffffffff82215900
// [157488.982180] fp->f_op phy addr is 0x2306e2828
// [157488.983535] fp->f_count is 1
// [157488.984594] read file 10 bytes, pos is hello worl
// [157490.029445] read file 10 bytes, pos is d.hello wo
// [157491.053499] read file 10 bytes, pos is rld.hello
// [157492.077289] read file 10 bytes, pos is world.hell
// [157493.101318] read file 10 bytes, pos is o world.he
// [157494.125395] read file 10 bytes, pos is llo world.
// [157495.149523] read file 10 bytes, pos is hello worl
// [157496.173495] read file 10 bytes, pos is d.hello wo
// [157497.197498] read file 10 bytes, pos is rld.hello
// [157498.221491] read file 10 bytes, pos is world.hell
// [157499.246395] read file 10 bytes, pos is o world.he
// [157500.269445] read file 10 bytes, pos is llo world.
// [157501.293365] read file 10 bytes, pos is hello worl
// [157502.317536] read file 10 bytes, pos is d.hello wo
// [157503.341403] read file 10 bytes, pos is rld.hello
// [157504.365703] read file 10 bytes, pos is world.hell
// [157505.389481] read file 10 bytes, pos is o world.he
// [157506.413397] read file 10 bytes, pos is llo world.
// [157507.437519] read file 10 bytes, pos is hello worl
// [157508.461234] read file 10 bytes, pos is d.hello wo
// [157509.485341] read file 10 bytes, pos is rld.hello
// [157510.509437] read file 10 bytes, pos is world.hell
// [157511.533470] read file 10 bytes, pos is o world.he
// [157512.557428] clear signal pending
// [157512.559924] read file 10 bytes, pos is llo world.
// [157513.581330] read file 10 bytes, pos is hello worl
// [157514.605551] read file 10 bytes, pos is d.hello wo
// [157515.629442] read file 10 bytes, pos is rld.hello
// [157516.653323] read file 10 bytes, pos is world.hell
// [157517.677330] read file 10 bytes, pos is o world.he
// [157518.701344] read file 10 bytes, pos is llo world.
// [157519.725429] read file 10 bytes, pos is hello worl
// [157520.749300] clear signal pending
// [157520.751495] read file 10 bytes, pos is d.hello wo
// [157521.773524] read file 10 bytes, pos is rld.hello
// [157522.797438] read file 10 bytes, pos is world.hell
// [157590.381437] read file 10 bytes, pos is world.hell
// [157591.405337] set signal pending
// Terminated

//
// # lsmod
// Module                  Size  Used by    Tainted: G
// kernel_read_4          24576  1
// # ps axu|grep insmod
//  1717 root     insmod kernel_read_4.ko
//  1720 root     grep insmod
// # kill 1717
// # kill 1717
// # kill -9 1717
// # lsmod
// Module                  Size  Used by    Tainted: G


// [158406.030654] open demo enter
// [158406.032759] &fp->f_op is ffff8882302e6228
// [158406.035314] fp->f_op is ffffffff8228e800
// [158406.038314] fp->f_op phy addr is 0x2302e6228
// [158406.042320] fp->f_count is 1
// [158406.044415] read file 10 bytes, pos is hello worl
// [158407.085439] read file 10 bytes, pos is d.hello wo
// [158408.109432] read file 10 bytes, pos is rld.hello
// [158409.133421] read file 10 bytes, pos is world.hell
// [158410.157514] read file 10 bytes, pos is o world.he
// [158411.181426] read file 10 bytes, pos is llo world.
// [158412.205462] read file 10 bytes, pos is hello worl
// [158413.229385] read file 10 bytes, pos is d.hello wo
// [158414.253554] read file 10 bytes, pos is rld.hello
// [158415.277435] read file 10 bytes, pos is world.hell
// [158416.301588] read file 10 bytes, pos is o world.he
// [158417.325444] read file 10 bytes, pos is llo world.
// [158418.349460] read file 10 bytes, pos is hello worl
// [158419.373427] read file 10 bytes, pos is d.hello wo
// [158420.397418] read file 10 bytes, pos is rld.hello
// [158421.421448] read file error -4, retry
// [158422.445437] read file error -4, retry
// [158423.469438] read file error -4, retry
// [158424.493620] read file error -4, retry
// [158425.517276] read file error -4, retry
// [158426.541276] read file error -4, retry
// [158427.565250] read file error -4, retry
// [158428.589451] read file error -4, retry
