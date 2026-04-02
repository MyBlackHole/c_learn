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

// # insmod k
// kernel_write_4_sig.ko  kthread_stop.ko        kthread_stop_err.ko
// # insmod kernel_write_4_sig.ko
// # ps axu|grep in
//     1 root     init
//  1501 root     /sbin/syslogd -n
//  1505 root     /sbin/klogd -n
//  1561 root     nginx: master process /usr/sbin/nginx
//  1562 www-data nginx: worker process
//  1566 root     sshd: /usr/sbin/sshd [listener] 0 of 10-100 startups
//  1584 root     insmod open_file_4.ko
//  1612 root     insmod kernel_write_4_sig.ko
//  1614 root     grep in
// # kill 1612
// [  508.054185] open demo enter
// [  508.056189] &fp->f_op is ffff88822d43ff28
// [  508.058969] fp->f_op is ffffffff82215900
// [  508.061587] fp->f_op phy addr is 0x22d43ff28
// [  508.064385] fp->f_count is 1
// [  508.066911] write file 12 bytes, pos is 12
// [  509.106987] write file 12 bytes, pos is 24
// [  510.131013] write file 12 bytes, pos is 36
// [  511.155038] write file 12 bytes, pos is 48
// [  512.179161] write file 12 bytes, pos is 60
// [  513.203074] write file 12 bytes, pos is 72
// [  514.227020] write file 12 bytes, pos is 84
// [  515.251085] write file 12 bytes, pos is 96
// [  516.274942] write file 12 bytes, pos is 108
// [  517.299073] write file 12 bytes, pos is 120
// [  518.323040] write file 12 bytes, pos is 132
// [  519.347045] write file 12 bytes, pos is 144
// [  520.370910] write file 12 bytes, pos is 156
// [  521.394978] write file error -4, retry
// [  522.419032] write file error -4, retry
// [  523.443018] write file error -4, retry
// [  524.467109] write file error -4, retry
// [  525.491048] write file error -4, retry
// [  526.515032] write file error -4, retry
// [  527.539025] write file error -4, retry
// [  528.563038] write file error -4, retry
// [  529.587029] write file error -4, retry
// [  530.611021] write file error -4, retry
// [  531.635008] write file error -4, retry
