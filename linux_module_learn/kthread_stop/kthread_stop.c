#include "linux/sched/task.h"
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

static struct task_struct *test_thread;

static int test_function(void *unused)
{
	allow_signal(SIGINT);
	while (!kthread_should_stop()) {
		pr_info("Waiting For Event...\n");
		// // 堵塞当前线程
		// msleep(1000000);
		// 堵塞当前线程，直到收到信号，可被信号打断
		msleep_interruptible(100);

		if (signal_pending(current)) {
			pr_info("Received signal, exiting...\n");
			break;
		}
	}

	pr_info("Thread exiting done.\n");

	return 0;
}

static int __init test_kthread_init(void)
{
	pr_info("[Hello] \n");

	// Create the kernel thread with name "MyWaitThread"
	test_thread = kthread_run(test_function, NULL, "my_test_kthread");
	if (IS_ERR(test_thread)) {
		pr_info("Thread creation failed\n");
		return PTR_ERR(test_thread);
	} else {
		pr_info("Thread created successfully\n");
		// 增加引用计数，防止线程退出时被回收
		get_task_struct(test_thread);
	}

	return 0;
}

static void __exit test_kthread_exit(void)
{
	if (!IS_ERR(test_thread)) {
		pr_info("Stopping the thread...\n");
		send_sig(SIGINT, test_thread, 1);
		msleep(1000);
		// // 等待线程结束
		// // 但是如果线程已退出，则会导致系统崩溃
		// kthread_stop(test_thread);

		// 释放引用计数, 同时释放线程结构体
		put_task_struct(test_thread);
	}
	pr_info("[Goodbye] mywaitqueue\n");
}

module_init(test_kthread_init);
module_exit(test_kthread_exit);
MODULE_LICENSE("GPL");
