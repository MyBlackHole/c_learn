#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cleanup.h>

static int __init scoped_guard_init(void)
{
	spinlock_t lock;
	spinlock_t s_lock;
	int tmp = 0;
	int tmp1 = 0;

	spin_lock_init(&lock);

	guard(spinlock_irq)(&lock);
	while (tmp > 100) {
		scoped_guard(spinlock_irq, &s_lock)
		{
			tmp1++;
		}
		tmp++;
	}

	return 0;
}

static void __exit scoped_guard_exit(void)
{
}

module_init(scoped_guard_init);
module_exit(scoped_guard_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lock");
