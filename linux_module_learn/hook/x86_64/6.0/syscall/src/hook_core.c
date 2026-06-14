// SPDX-License-Identifier: GPL-2.0-only
/*
 * hook_core.c - module init/exit, hook registration framework
 *
 * Copyright (C) 2026 hook_system_call contributors
 *
 * Top-level module entry point.  Resolves kernel symbols, registers
 * all four write-family syscall hooks, creates the relay channel,
 * and exposes module parameters for runtime control.
 */

#define pr_fmt(fmt) "hw: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/err.h>
#include <generated/utsrelease.h>
#include <linux/delay.h>

#include "flipswitch.h"
#include "events.h"

/* ------------------------------------------------------------------ */
/* Module state (single global instance)                                */
/* ------------------------------------------------------------------ */

/*
 * Global module state.  Non-static so that relay_chan.c, control.c,
 * and security.c can access it via extern declarations.
 */
struct hw_state hw_state;

/* ------------------------------------------------------------------ */
/* Hook registration                                                   */
/* ------------------------------------------------------------------ */

int hw_register_hooks(struct hw_state *state)
{
	int i;
	int ret = 0;

	mutex_lock(&state->lock);

	for (i = 0; i < state->nr_hooks; i++) {
		struct hw_hook *hook = &state->hooks[i];
		unsigned long *table;
		void *call_site;

		/*
		 * Read the original syscall function address from the
		 * sys_call_table.  The table is __ro_after_init so we
		 * can read but not write it; that is exactly what we need.
		 */
		table = (unsigned long *)state->sys_call_table_addr;
		hook->orig_fn = table[hook->syscall_nr];

		if (!hook->orig_fn) {
			pr_err("syscall %d has NULL entry in sys_call_table\n",
			       hook->syscall_nr);
			ret = -ENOENT;
			goto out;
		}

		/* Locate the call instruction in x64_sys_call(). */
		call_site = hw_scan_x64_sys_call(state, hook->orig_fn);
		if (!call_site) {
			pr_err("cannot find call for syscall %d (%s)\n",
			       hook->syscall_nr, hook->name);
			ret = -ENOENT;
			goto out;
		}

		/* Save the original offset for later restoration. */
		memcpy(&hook->patch.orig_offset, call_site + 1,
		       sizeof(hook->patch.orig_offset));
		hook->patch.call_addr = call_site;
		hook->patch.applied   = false;

		/*
		 * Apply the patch if this hook is enabled by hook_mask.
		 * hw_hook_mask defaults to HW_MASK_ALL (all writable), which
		 * can be overridden via the module parameter or at runtime.
		 */
		if (hw_control_is_enabled(hook->syscall_nr)) {
			hw_apply_patch(&hook->patch, hook->hook_fn);
			hook->enabled = true;
		}

		pr_info("hooked %s (nr=%d, orig=0x%lx, call=%p, enabled=%d)\n",
			hook->name, hook->syscall_nr, hook->orig_fn,
			call_site, hook->enabled);
	}

out:
	mutex_unlock(&state->lock);
	return ret;
}

void hw_unregister_hooks(struct hw_state *state)
{
	int i;

	mutex_lock(&state->lock);

	for (i = 0; i < state->nr_hooks; i++) {
		struct hw_hook *hook = &state->hooks[i];

		if (!hook->patch.applied)
			continue;

		hw_restore_patch(&hook->patch);
		hook->enabled = false;

		pr_info("restored %s (nr=%d)\n", hook->name,
			hook->syscall_nr);
	}

	memset(state->hooks, 0, sizeof(state->hooks));
	state->nr_hooks = 0;

	mutex_unlock(&state->lock);
}

/* ------------------------------------------------------------------ */
/* Build assertions                                                    */
/* ------------------------------------------------------------------ */

void hw_assert_build(void)
{
	/* 8+4+4+4+4+8+8+16+256 = 312 (__packed, no padding) */
	BUILD_BUG_ON(sizeof(struct syscall_event) != 312);
}

/* ------------------------------------------------------------------ */
/* Module init / exit                                                  */
/* ------------------------------------------------------------------ */

static int __init hw_module_init(void)
{
	int ret;

	pr_info("loading (target kernel %s)\n", UTS_RELEASE);

	hw_security_init(&hw_state);
	hw_control_init(&hw_state);
	hw_assert_build();

	/* Step 1: resolve kernel symbols */
	ret = hw_find_symbols(&hw_state);
	if (ret)
		goto err_syms;

	/* Step 2: initialise relayfs channel */
	ret = hw_relay_init(&hw_state);
	if (ret)
		goto err_relay;

	/* Step 3: register write-family hooks */
	ret = hw_init_write_hooks(&hw_state);
	if (ret)
		goto err_hooks;

	/* Step 4: apply all patches */
	ret = hw_register_hooks(&hw_state);
	if (ret) {
		/*
		 * hw_register_hooks may have partially applied
		 * patches before hitting the error.  Unregister
		 * restores those patches so that no call site in
		 * x64_sys_call() still points into module memory
		 * after the module is unloaded.
		 */
		hw_unregister_hooks(&hw_state);
		goto err_patch;
	}

	/* Step 5: sync original function pointers into hook_write.c */
	hw_sync_orig_ptrs(&hw_state);

	pr_info("loaded successfully\n");
	return 0;

err_patch:
	hw_exit_write_hooks(&hw_state);
err_hooks:
	hw_relay_exit(&hw_state);
err_relay:
err_syms:
	hw_control_exit(&hw_state);
	hw_security_exit(&hw_state);
	return ret;
}

static void __exit hw_module_exit(void)
{
	pr_info("unloading\n");

	/*
	 * Step 1: flag unloading so that hw_security_event_begin() knows
	 * we are shutting down (reserved for future denials).
	 */
	hw_state.unloading = true;
	smp_mb();

	/*
	 * Step 2: restore all FlipSwitch patches FIRST.
	 * After this, new syscall invocations go directly to the
	 * original kernel functions — no hook handler can be entered.
	 */
	hw_unregister_hooks(&hw_state);

	/*
	 * Step 3: wait for any hook handlers that were already
	 * in-flight before the patch restoration to drain.
	 * Because patches are already restored, no new handlers
	 * can appear, so the in_flight counter will eventually
	 * reach zero and stay there.
	 */
	while (atomic_read(&hw_state.in_flight) > 0)
		usleep_range(100, 200);
	synchronize_rcu();

	/* Step 4: clean up sub-systems (no handlers remain) */
	hw_exit_write_hooks(&hw_state);
	hw_relay_exit(&hw_state);
	hw_control_exit(&hw_state);
	hw_security_exit(&hw_state);

	pr_info("unloaded cleanly\n");
}

module_init(hw_module_init);
module_exit(hw_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hook_system_call contributors");
MODULE_DESCRIPTION("FlipSwitch-based syscall hook for Linux 7.0+ (write-family)");
MODULE_VERSION("0.1.0");
