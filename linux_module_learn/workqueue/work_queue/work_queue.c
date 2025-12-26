#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/time64.h>
#include <linux/of.h>
#include <linux/completion.h>
#include <linux/mfd/core.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

struct work_struct work_demo;
struct work_struct work_demo2;

struct workqueue_struct *workqueue_demo = NULL;

struct proc_dir_entry *workqueue_proc = NULL;

static void work_demo_func(struct work_struct *work)
{
	pr_info("cpu id = %d,taskname = %s\n", raw_smp_processor_id(),
		current->comm);
	mdelay(1000 * 10);
}

static int workqueue_proc_show(struct seq_file *seq_file, void *data)
{
	pr_info("cpu id = %d\n", raw_smp_processor_id());

	/* 推送到系统工作队列 */
	schedule_work(&work_demo);

	return 0;
}

static int workqueue_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, workqueue_proc_show, NULL);
}

static ssize_t workqueue_proc_store(struct file *file,
				    const char __user *buffer, size_t count,
				    loff_t *ppos)
{
	int ret;
	char *buf;
	buf = kmalloc(count, GFP_KERNEL);
	if (!buf) {
		return -1;
	}

	ret = copy_from_user(buf, buffer, count);
	if (ret < 0) {
		return ret;
	}

	if (buf[0] == '1') {
		pr_info("work_demo,cpu id = %d\n", raw_smp_processor_id());
		pr_info("queue work_demo end, ret: %d\n",
			queue_work(workqueue_demo, &work_demo));
	} else if (buf[0] == '2') {
		pr_info("work_demo2,cpu id = %d\n", raw_smp_processor_id());
		pr_info("queue work_demo end, ret: %d\n",
			queue_work(workqueue_demo, &work_demo2));
	}
	kfree(buf);

	return count;
}

static const struct proc_ops workqueue_proc_fops = {
	.proc_open = workqueue_proc_open,
	.proc_read = seq_read,
	.proc_write = workqueue_proc_store,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init workqueue_test_init(void)
{
	INIT_WORK(&work_demo, work_demo_func);
	INIT_WORK(&work_demo2, work_demo_func);
	workqueue_demo = alloc_workqueue("workqueue_demo", WQ_UNBOUND, 2);
	if (!workqueue_demo) {
		pr_err("alloc_workqueue failed\n");
		return -ENOMEM;
	}

	workqueue_proc =
		proc_create("workqueue", 0, NULL, &workqueue_proc_fops);
	if (!workqueue_proc) {
		destroy_workqueue(workqueue_demo);
		workqueue_demo = NULL;
		pr_err("proc_create failed\n");
		return -ENOMEM;
	}
	return 0;
}

static void __exit workqueue_test_exit(void)
{
	if (workqueue_demo) {
		destroy_workqueue(workqueue_demo);
		workqueue_demo = NULL;
	}

	if (workqueue_proc) {
		proc_remove(workqueue_proc);
		workqueue_proc = NULL;
	}

	pr_info("workqueue_test exit\n");

	return;
}

MODULE_LICENSE("GPL");
module_init(workqueue_test_init);
module_exit(workqueue_test_exit);


// ❯ sudo insmod work_queue.ko
// ❯ lsmod | grep work_
// work_queue             16384  0
//
// ❯ ls -alh /proc/workqueue
// Permissions Size User Date Modified Name
// .r--r--r--     0 root 25 12月 11:47 󰡯 /proc/workqueue
//
// ❯ sudo su
//
// [root@black work_queue]# echo 1 > /proc/workqueue
// [root@black work_queue]# echo 2 > /proc/workqueue
// [root@black work_queue]# echo 2 > /proc/workqueue
// [root@black work_queue]# echo 1 > /proc/workqueue
// [root@black work_queue]# nvim /proc/workqueue
// [root@black work_queue]# nvim /proc/workqueue
//
// [261431.541112] work_demo,cpu id = 8
// [261431.541128] queue work_demo end, ret: 1
// [261431.541136] cpu id = 10,taskname = kworker/u66:4
// [261441.286215] work_demo2,cpu id = 2
// [261441.286225] queue work_demo end, ret: 1
// [261441.286268] cpu id = 4,taskname = kworker/u65:4
// [261443.940833] work_demo2,cpu id = 4
// [261443.940841] queue work_demo end, ret: 1
// [261451.296132] cpu id = 8,taskname = kworker/u65:4
// [261454.292641] work_demo,cpu id = 5
// [261454.292658] queue work_demo end, ret: 1
// [261454.292677] cpu id = 2,taskname = kworker/u65:1
// [261477.390647] cpu id = 13
// [261477.390665] cpu id = 13,taskname = kworker/13:0
// [261483.928218] cpu id = 2
// [261487.400771] cpu id = 13,taskname = kworker/13:0
