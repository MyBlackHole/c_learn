// SPDX-License-Identifier: GPL-2.0-only
/*
 * flipswitch_core.c - scan x64_sys_call(), apply / restore patches
 *
 * The FlipSwitch technique works by patching the 'call rel32'
 * instruction (opcode 0xE8) inside the x64_sys_call() switch-
 * statement dispatcher.  On Linux 6.9+ the dispatcher no longer
 * indexes into sys_call_table; instead each syscall is compiled as
 * a switch-case with a direct call.  By modifying the relative
 * offset of that call we redirect execution to our hook function.
 *
 * CR0.WP note: we deliberately avoid native_write_cr0() on kernels
 * >= 7.0 because that function has been hardened with a __SCT__WARN_trap
 * that fires when clearing the WP bit.  Instead we follow the
 * FlipSwitch reference project approach and write CR0 via direct
 * inline asm (write_cr0_forced), bypassing the kernel wrapper.
 */

#define pr_fmt(fmt) "hw: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/stop_machine.h>
#include <asm/tlbflush.h>
#include <asm/sync_core.h>

#include "flipswitch.h"

/* ------------------------------------------------------------------ */
/* CR0 helpers — direct inline asm (bypasses hardened native_write_cr0) */
/* ------------------------------------------------------------------ */

static inline unsigned long hw_read_cr0(void)
{
	unsigned long val;
	asm volatile("mov %%cr0, %0" : "=r"(val));
	return val;
}

static inline void hw_write_cr0(unsigned long val)
{
	asm volatile("mov %0, %%cr0" : "+r"(val) : : "memory");
}

static void hw_cr0_disable_wp(void)
{
	hw_write_cr0(hw_read_cr0() & ~X86_CR0_WP);
}

static void hw_cr0_enable_wp(void)
{
	hw_write_cr0(hw_read_cr0() | X86_CR0_WP);
}

/* ------------------------------------------------------------------ */
/* Symbol resolution (via kprobe)                                       */
/* ------------------------------------------------------------------ */

/*
 * kallsyms_on_each_symbol() and kallsyms_lookup_name() are no longer
 * exported to modules in Linux 7.0.  However kallsyms_lookup_name is
 * still present in the kernel image.  We locate it by registering a
 * temporary kprobe, which resolves the symbol name via its internal
 * kallsyms walker, then call it directly as a function pointer.
 */
typedef unsigned long (*hw_kallsyms_lookup_fn)(const char *);

static hw_kallsyms_lookup_fn hw_kallsyms_lookup;

static int hw_resolve_lookup_fn(void)
{
	struct kprobe kp = {
		.symbol_name = "kallsyms_lookup_name",
	};
	int ret;

	ret = register_kprobe(&kp);
	if (ret < 0)
		return ret;

	hw_kallsyms_lookup = (hw_kallsyms_lookup_fn)kp.addr;
	unregister_kprobe(&kp);
	return 0;
}

int hw_find_symbols(struct hw_state *state)
{
	int ret;

	ret = hw_resolve_lookup_fn();
	if (ret < 0) {
		pr_err("cannot find kallsyms_lookup_name via kprobe\n");
		return ret;
	}

	state->x64_sys_call_addr = hw_kallsyms_lookup("x64_sys_call");
	if (!state->x64_sys_call_addr)
		state->x64_sys_call_addr = hw_kallsyms_lookup("__x64_sys_call");

	if (!state->x64_sys_call_addr) {
		pr_err("cannot find x64_sys_call in kallsyms\n");
		return -ENXIO;
	}

	state->sys_call_table_addr = hw_kallsyms_lookup("sys_call_table");
	if (!state->sys_call_table_addr) {
		pr_err("cannot find sys_call_table in kallsyms\n");
		return -ENXIO;
	}

	pr_info("x64_sys_call=0x%lx sys_call_table=0x%lx\n",
		state->x64_sys_call_addr, state->sys_call_table_addr);
	return 0;
}

/* ------------------------------------------------------------------ */
/* Scanning x64_sys_call() for call rel32 matching a target address    */
/* ------------------------------------------------------------------ */

void *hw_scan_x64_sys_call(struct hw_state *state, unsigned long target)
{
	unsigned long start = state->x64_sys_call_addr;
	unsigned long end   = start + HW_SCAN_SIZE;
	unsigned long addr;

	if (!start) {
		pr_err("x64_sys_call address not resolved\n");
		return NULL;
	}

	for (addr = start; addr < end - 5; addr++) {
		u8 opcode;

		opcode = *(volatile u8 *)addr;
		/*
		 * Linux 7.0 compiles x64_sys_call() dispatch entries as
		 * tail-call jmp rel32 (opcode 0xE9) rather than call rel32
		 * (0xE8) for efficiency.  We match both forms since the
		 * offset calculation (addr + 5 + rel32) is identical.
		 */
		if (opcode != 0xE9 && opcode != 0xE8)
			continue;

		{
			int32_t rel;
			unsigned long call_target;

			memcpy(&rel, (const void *)(addr + 1), sizeof(rel));
			call_target = addr + 5 + rel;

			if (call_target == target) {
				pr_debug("found call at 0x%lx -> 0x%lx\n",
					 addr, call_target);
				return (void *)addr;
			}
		}
	}

	pr_err("no call to 0x%lx found in x64_sys_call (scan %d bytes)\n",
	       target, HW_SCAN_SIZE);
	return NULL;
}

/* ------------------------------------------------------------------ */
/* Patching                                                            */
/* ------------------------------------------------------------------ */

static void hw_do_patch(struct hw_patch *patch, void *target_fn)
{
	int32_t new_offset;
	void *call_addr = patch->call_addr;

	if (WARN_ON(!call_addr))
		return;

	new_offset = (void *)target_fn - call_addr - 5;

	pr_debug("patching %p: orig_offset=%d new_offset=%d\n",
		 call_addr, patch->orig_offset, new_offset);

	/* FlipSwitch: disable WP, write offset, re-enable WP, flush. */
	hw_cr0_disable_wp();
	memcpy(call_addr + 1, &new_offset, sizeof(new_offset));
	hw_cr0_enable_wp();

	wmb();
	sync_core();
	kick_all_cpus_sync();
}

void hw_apply_patch(struct hw_patch *patch, void *hook_fn)
{
	if (!patch || !hook_fn)
		return;

	if (patch->applied)
		return;

	hw_do_patch(patch, hook_fn);
	patch->applied = true;
}

void hw_restore_patch(struct hw_patch *patch)
{
	unsigned long target;

	if (!patch)
		return;

	if (!patch->applied)
		return;

	target = (unsigned long)patch->call_addr + 5 + patch->orig_offset;
	hw_do_patch(patch, (void *)target);
	patch->applied = false;
}
