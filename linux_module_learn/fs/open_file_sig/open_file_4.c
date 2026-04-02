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
	int count = 1000;
	char path[1024];

	while (count--) {
		snprintf(path, 1024, "/tmp/file_test%d", count);
		fp = filp_open(path, O_RDWR | O_CREAT, 0644);
		if (IS_ERR(fp)) {
			printk("create file error\n");
			return -1;
		}
		filp_close(fp, NULL);
		fp = NULL;

		msleep(1000);
	}

	return 0;
}

void __exit my_exit(void)
{
	pr_info("open demo exit\n");
	if (fp) {
		filp_close(fp, NULL);
	}
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");


// 打开关闭文件不受信号影响
// # kill -9 1584
// # kill -9 1584
// # kill -9 1584
// # kill -9 1584
// # kill -9 1584
// # kill -1584
// sh: bad signal name '1584'
// # kill 1584
// # ps axu|grep ins
//  1584 root     insmod open_file_4.ko
//  1599 root     grep ins
// # kill 1584
// # kill 1584
// # kill 1584
// # kill 1584
// # kill 1584
