// SPDX-License-Identifier: GPL-2.0-only
/*
 * control.c - module parameters and runtime control
 *
 * Exposes a hook_mask module parameter that controls which syscalls
 * are actively hooked.  Writing to hook_mask at runtime enables or
 * disables individual hooks by applying/restoring FlipSwitch patches.
 */

#define pr_fmt(fmt) "hw: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/syscalls.h>

#include "flipswitch.h"

static unsigned int hw_hook_mask = HW_MASK_ALL;

extern struct hw_state hw_state;

static int hw_param_set_hook_mask(const char *val,
				  const struct kernel_param *kp)
{
	unsigned int new_mask;
	int ret;
	int i;

	ret = kstrtouint(val, 0, &new_mask);
	if (ret)
		return ret;

	if (new_mask > HW_MASK_ALL)
		return -EINVAL;

	mutex_lock(&hw_state.lock);

	for (i = 0; i < hw_state.nr_hooks; i++) {
		struct hw_hook *hook = &hw_state.hooks[i];
		unsigned int bit;

		switch (hook->syscall_nr) {
		case __NR_write:    bit = HW_BIT_WRITE;    break;
		case __NR_writev:   bit = HW_BIT_WRITEV;   break;
		case __NR_pwrite64: bit = HW_BIT_PWRITE64; break;
		case __NR_pwritev:
		case __NR_pwritev2: bit = HW_BIT_PWRITEV2; break;
		default:            continue;
		}

		if (!hook->patch.call_addr)
			continue;

		if ((new_mask & BIT(bit)) && !hook->enabled) {
			hw_apply_patch(&hook->patch, hook->hook_fn);
			hook->enabled = true;
			pr_info("enabled hook for %s\n", hook->name);
		} else if (!(new_mask & BIT(bit)) && hook->enabled) {
			hw_restore_patch(&hook->patch);
			hook->enabled = false;
			pr_info("disabled hook for %s\n", hook->name);
		}
	}

	hw_hook_mask = new_mask;
	mutex_unlock(&hw_state.lock);
	return 0;
}

static struct kernel_param_ops hw_hook_mask_ops = {
	.set = hw_param_set_hook_mask,
	.get = param_get_uint,
};

module_param_cb(hook_mask, &hw_hook_mask_ops, &hw_hook_mask, 0644);
MODULE_PARM_DESC(hook_mask,
		 "Bitmask: bit0=write, bit1=writev, bit2=pwrite64, bit3=pwritev2");

bool hw_control_is_enabled(unsigned int syscall_nr)
{
	switch (syscall_nr) {
	case __NR_write:    return hw_hook_mask & HW_MASK_WRITE;
	case __NR_writev:   return hw_hook_mask & HW_MASK_WRITEV;
	case __NR_pwrite64: return hw_hook_mask & HW_MASK_PWRITE64;
	case __NR_pwritev:
	case __NR_pwritev2: return hw_hook_mask & HW_MASK_PWRITEV2;
	default:            return false;
	}
}

void hw_control_init(struct hw_state *state)
{
	mutex_init(&state->lock);
}

void hw_control_exit(struct hw_state *state)
{
	mutex_destroy(&state->lock);
}
