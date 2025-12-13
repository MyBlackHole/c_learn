#include "arch.h"

#include <linux/mman.h>
#include <linux/syscalls.h> // set_fs, get_fs
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/compat.h>

typedef asmlinkage long (*sys_call_funcptr_t)(const struct pt_regs *);

#if defined(__NR_chmod)
sys_call_funcptr_t original_chmod = NULL;
#endif // __NR_chmod
#if defined(__NR_fchmodat)
sys_call_funcptr_t original_fchmodat = NULL;
#endif // __NR_fchmodat

static void **sys_call_table_ptr = NULL;

#define DEFINE_BACKUP_PATH "/var/xxxxxxxxxxxxx/"

// static int copy_kernel_string_to_user(const char *kernel_str,
// 				      char __user **user_str)
// {
// 	size_t len;
// 	int ret;
//
// 	if (!kernel_str) {
// 		return -EINVAL;
// 	}
//
// 	len = strlen(kernel_str) + 1;
//
// 	*user_str = (char __user *)compat_alloc_user_space(len);
// 	if (!*user_str) {
// 		pr_err("error: alloc user space failed.\n");
// 		return -ENOMEM;
// 	}
//
// 	ret = copy_to_user(*user_str, kernel_str, len);
// 	if (ret) {
// 		pr_err("error: copy to user space failed, ret %d.\n", ret);
// 		return -EFAULT;
// 	}
//
// 	return 0;
// }

static int copy_kernel_string_to_user2(const char *kernel_str,
				       unsigned long *user_addr,
				       char __user **user_str)
{
	unsigned long max_addr;

	max_addr = user_addr_max();
	*user_addr = vm_mmap(NULL, 0, PAGE_SIZE * 2,
			     PROT_READ | PROT_WRITE | PROT_EXEC,
			     MAP_ANONYMOUS | MAP_PRIVATE, 0);
	if (*user_addr >= (unsigned long)(TASK_SIZE)) {
		pr_warn("Failed to allocate user memory\n");
		return -ENOMEM;
	}

	pr_info("TASK_SIZE: %lu, user_addr: %lu, max_addr: %lu\n",
		(unsigned long)(TASK_SIZE), *user_addr, max_addr);

	*user_str = (char __user *)*user_addr;
	if (copy_to_user(*user_str, kernel_str, strlen(kernel_str) + 1)) {
		pr_err("error: copy to user space failed.\n");
		vm_munmap(*user_addr, PAGE_SIZE * 2);
		return -EFAULT;
	}
	return 0;
}

static int sys_chmod(const char *filename, umode_t mode)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 0, 0)
	struct pt_regs regs;
#endif
	long ret = 0;
	char __user *user_filename = NULL;
	unsigned long user_addr;

	if (copy_kernel_string_to_user2(filename, &user_addr, &user_filename) !=
	    0) {
		pr_err("error: copy kernel string to user space failed\n");
		return -EFAULT;
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 0, 0)
	memset(&regs, 0x00, sizeof(regs));

#if defined(__NR_chmod)
	PT_REGS_PARM1(&regs) = (unsigned long)user_filename;
	PT_REGS_PARM2(&regs) = (unsigned long)mode;
	ret = (*original_chmod)(&regs);
	pr_info("info: call chmod %s, mode:%d, ret:%ld\n", filename, mode, ret);
#else
	PT_REGS_PARM1(&regs) = (unsigned long)AT_FDCWD;
	PT_REGS_PARM2(&regs) = (unsigned long)user_filename;
	PT_REGS_PARM3(&regs) = (unsigned long)mode;
	PT_REGS_PARM4(&regs) = (unsigned long)0;
	ret = (*original_fchmodat)(&regs);
	pr_info("info: call fchmodat %s, mode:%d, ret:%ld\n", filename, mode,
		ret);
#endif

#else // kernel_version

#if defined(__NR_chmod)
	ret = (*original_chmod)(filename, mode);
#else
	ret = (*original_fchmodat)(AT_FDCWD, filename, mode, 0);
#endif

#endif
	if (ret != 0) {
		pr_info("error: chmod %s failure! ret:%ld\n", filename, ret);
	}

	vm_munmap(user_addr, PAGE_SIZE * 2);

	return ret;
}

static int __init api_test_init(void)
{
	long ret = 0;

	pr_info("info: init api test module.\n");

	sys_call_table_ptr = (void **)kallsyms_lookup_name("sys_call_table");
	if (sys_call_table_ptr == NULL) {
		pr_err("error: get sys call table failure.\n");
		return -EINVAL;
	}

#if defined(__NR_chmod)
	original_chmod = sys_call_table_ptr[__NR_chmod];
	pr_info("info: get system call chmod.\n");
#endif
#if defined(__NR_fchmodat)
	original_fchmodat = sys_call_table_ptr[__NR_fchmodat];
	pr_info("info: get system call fchmodat.\n");
#endif

	if (original_chmod == NULL && original_fchmodat == NULL) {
		pr_err("error: get system call failure.\n");
	} else {
		pr_info("info: hook system call chmod/fchmodat success.\n");
		if (sys_chmod(DEFINE_BACKUP_PATH, 0777) != 0) {
			pr_info("error: test chmod failure.\n");
		} else {
			pr_info("info: test chmod success.\n");
		}
	}
	return ret;
}

static void __exit api_test_exit(void)
{
	pr_info("info: exit api test module.\n");
}

module_init(api_test_init);
module_exit(api_test_exit);

MODULE_LICENSE("GPL");
