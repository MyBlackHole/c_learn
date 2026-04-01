#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/sched/task.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>

static struct task_struct *m_th;

DECLARE_COMPLETION(done);

static int test_function(void *unused)
{
	pr_info("Thread exiting done.\n");
	char *file = "/tmp/file_test";
	struct file *fp = filp_open(file, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		pr_err("open file error %ld\n", PTR_ERR(fp));
		fp = NULL;
		return -1;
	} else {
		pr_info("open file success\n");
		unsigned char buf1[12] = "hello world.";
		if (kernel_write(fp, buf1, sizeof(buf1), &fp->f_pos) < 0) {
			pr_err("write file error\n");
		}
		if (filp_close(fp, NULL) != 0) {
			fp = NULL;
			pr_err("close file error\n");
		} else {
			pr_info("close file success\n");
		}
	}
	complete(&done);
	return 0;
}

static int __init test_kthread_init(void)
{
	pr_info("[Hello] \n");

	m_th = kthread_run(test_function, NULL, "test_kthread_open_file");
	if (IS_ERR(m_th)) {
		pr_info("Thread creation failed\n");
		return PTR_ERR(m_th);
	} else {
		pr_info("Thread created successfully\n");
		// 增加引用计数，防止线程退出时被回收
		get_task_struct(m_th);
	}

	return 0;
}

static void __exit test_kthread_exit(void)
{
	wait_for_completion(&done);
	put_task_struct(m_th);
	pr_info("[Goodbye]\n");
}

module_init(test_kthread_init);
module_exit(test_kthread_exit);
MODULE_LICENSE("GPL");
