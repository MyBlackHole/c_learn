// SPDX-License-Identifier: GPL-2.0-only
/*
 * hook_write.c - hook handlers for write-family syscalls
 *
 * Each hook function extracts userspace arguments from struct pt_regs,
 * records an event via relayfs, then forwards the call to the original
 * syscall implementation.
 *
 * x86_64 syscall register layout (CONFIG_ARCH_HAS_SYSCALL_WRAPPER):
 *   regs->di  = arg1 (fd)
 *   regs->si  = arg2 (buf / iov)
 *   regs->dx  = arg3 (count / iovcnt)
 *   regs->r10 = arg4 (offset for pwrite64/pwritev2)
 */

#define pr_fmt(fmt) "hw: " fmt

#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/uio.h>
#include <linux/uaccess.h>

#include "flipswitch.h"
#include "events.h"

/* ------------------------------------------------------------------ */
/* Per-syscall original function pointers (lock-free read from hot path) */
/* ------------------------------------------------------------------ */

static long (*orig_write)(struct pt_regs *);
static long (*orig_writev)(struct pt_regs *);
static long (*orig_pwrite64)(struct pt_regs *);
static long (*orig_pwritev)(struct pt_regs *);
static long (*orig_pwritev2)(struct pt_regs *);

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Event recording                                                     */
/* ------------------------------------------------------------------ */

static void hw_record_event(unsigned int nr, unsigned int fd,
			    u64 count, u64 offset)
{
	struct syscall_event ev;

	if (!hw_control_is_enabled(nr))
		return;

	memset(&ev, 0, sizeof(ev));
	ev.timestamp_ns = ktime_get_ns();
	ev.pid  = task_tgid_nr(current);
	ev.tid  = task_pid_nr(current);
	ev.syscall_nr = nr;
	ev.fd   = fd;
	ev.count = count;
	ev.offset = offset;
	get_task_comm(ev.comm, current);

	hw_resolve_fd_path(fd, ev.path, sizeof(ev.path));
	hw_relay_write_event(&ev);
}

static u64 hw_iovec_total_len(const struct iovec __user *iov,
			      unsigned long iovcnt)
{
	u64 total = 0;
	struct iovec entry;
	unsigned long i;

	if (!iov || !iovcnt)
		return 0;

	for (i = 0; i < iovcnt; i++) {
		if (copy_from_user(&entry, &iov[i], sizeof(entry)))
			break;
		total += entry.iov_len;
	}
	return total;
}

/* ------------------------------------------------------------------ */
/* Hook: __x64_sys_write(fd, buf, count)                               */
/* ------------------------------------------------------------------ */

static long hw___x64_sys_write(struct pt_regs *regs)
{
	unsigned int fd    = (unsigned int)regs->di;
	u64 count   = (u64)regs->dx;
	long ret;

	hw_security_event_begin();
	hw_record_event(__NR_write, fd, count, 0);
	ret = orig_write(regs);
	hw_security_event_end();
	return ret;
}

/* ------------------------------------------------------------------ */
/* Hook: __x64_sys_writev(fd, iov, iovcnt)                             */
/* ------------------------------------------------------------------ */

static long hw___x64_sys_writev(struct pt_regs *regs)
{
	unsigned int fd       = (unsigned int)regs->di;
	const struct iovec __user *iov = (const void *)regs->si;
	unsigned long iovcnt  = (unsigned long)regs->dx;
	u64 total_len         = hw_iovec_total_len(iov, iovcnt);
	long ret;

	hw_security_event_begin();
	hw_record_event(__NR_writev, fd, total_len, 0);
	ret = orig_writev(regs);
	hw_security_event_end();
	return ret;
}

/* ------------------------------------------------------------------ */
/* Hook: __x64_sys_pwrite64(fd, buf, count, offset)                    */
/* ------------------------------------------------------------------ */

static long hw___x64_sys_pwrite64(struct pt_regs *regs)
{
	unsigned int fd  = (unsigned int)regs->di;
	u64 count = (u64)regs->dx;
	u64 offset = (u64)regs->r10;
	long ret;

	hw_security_event_begin();
	hw_record_event(__NR_pwrite64, fd, count, offset);
	ret = orig_pwrite64(regs);
	hw_security_event_end();
	return ret;
}

/* ------------------------------------------------------------------ */
/* Hook: __x64_sys_pwritev / pwritev2                                   */
/* ------------------------------------------------------------------ */

static long hw___x64_sys_pwritev(struct pt_regs *regs)
{
	unsigned int fd       = (unsigned int)regs->di;
	const struct iovec __user *iov = (const void *)regs->si;
	unsigned long iovcnt  = (unsigned long)regs->dx;
	loff_t offset         = (loff_t)regs->r10;
	u64 total_len         = hw_iovec_total_len(iov, iovcnt);
	long ret;

	hw_security_event_begin();
	hw_record_event(__NR_pwritev, fd, total_len, (u64)offset);
	ret = orig_pwritev(regs);
	hw_security_event_end();
	return ret;
}

static long hw___x64_sys_pwritev2(struct pt_regs *regs)
{
	unsigned int fd       = (unsigned int)regs->di;
	const struct iovec __user *iov = (const void *)regs->si;
	unsigned long iovcnt  = (unsigned long)regs->dx;
	loff_t offset         = (loff_t)regs->r10;
	u64 total_len         = hw_iovec_total_len(iov, iovcnt);
	long ret;

	hw_security_event_begin();
	hw_record_event(__NR_pwritev2, fd, total_len, (u64)offset);
	ret = orig_pwritev2(regs);
	hw_security_event_end();
	return ret;
}

/* ------------------------------------------------------------------ */
/* Registration helpers                                                */
/* ------------------------------------------------------------------ */

/*
 * hw_init_write_hooks - populate the hook array with write-family entries.
 * Called from hook_core.c during module_init.
 */
int hw_init_write_hooks(struct hw_state *state)
{
	if (state->nr_hooks + 5 > HW_MAX_HOOKS)
		return -ENOSPC;

#define ADD(_nr, _name)							\
	do {								\
		state->hooks[state->nr_hooks].syscall_nr = _nr;		\
		state->hooks[state->nr_hooks].hook_fn    =		\
			hw___x64_sys_ ## _name;				\
		state->hooks[state->nr_hooks].enabled    = false;	\
		snprintf(state->hooks[state->nr_hooks].name,		\
			 sizeof(state->hooks[state->nr_hooks].name),	\
			 "__x64_sys_%s", #_name);			\
		state->nr_hooks++;					\
	} while (0)

	ADD(__NR_write,    write);
	ADD(__NR_writev,   writev);
	ADD(__NR_pwrite64, pwrite64);
	ADD(__NR_pwritev,  pwritev);
	ADD(__NR_pwritev2, pwritev2);

#undef ADD
	return 0;
}

/*
 * hw_sync_orig_ptrs - copy original function addresses from the
 * registered hook array into the per-syscall static pointers.
 *
 * Called from hw_register_hooks() in hook_core.c after all patches
 * are successfully applied and the orig_fn fields are filled in.
 */
void hw_sync_orig_ptrs(struct hw_state *state)
{
	int i;

	for (i = 0; i < state->nr_hooks; i++) {
		switch (state->hooks[i].syscall_nr) {
		case __NR_write:
			orig_write = (typeof(orig_write))
				state->hooks[i].orig_fn;
			break;
		case __NR_writev:
			orig_writev = (typeof(orig_writev))
				state->hooks[i].orig_fn;
			break;
		case __NR_pwrite64:
			orig_pwrite64 = (typeof(orig_pwrite64))
				state->hooks[i].orig_fn;
			break;
		case __NR_pwritev:
			orig_pwritev = (typeof(orig_pwritev))
				state->hooks[i].orig_fn;
			break;
		case __NR_pwritev2:
			orig_pwritev2 = (typeof(orig_pwritev2))
				state->hooks[i].orig_fn;
			break;
		}
	}
}

void hw_exit_write_hooks(struct hw_state *state)
{
	orig_write    = NULL;
	orig_writev   = NULL;
	orig_pwrite64 = NULL;
	orig_pwritev  = NULL;
	orig_pwritev2 = NULL;
}
