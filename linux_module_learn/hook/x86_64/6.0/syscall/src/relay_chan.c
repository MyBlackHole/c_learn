// SPDX-License-Identifier: GPL-2.0-only
/*
 * relay_chan.c - relayfs channel management
 *
 * Creates a relay channel under /sys/kernel/debug/hook_write/events
 * for high-throughput event streaming to userspace.
 */

#define pr_fmt(fmt) "hw: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/relay.h>
#include <linux/debugfs.h>
#include <linux/err.h>

#include "flipswitch.h"
#include "events.h"

static struct dentry *hw_debug_dir;

/*
 * Global state reference (defined in hook_core.c).
 * We use it here to access the relay channel pointer.
 */
extern struct hw_state hw_state;

/* ------------------------------------------------------------------ */
/* relay_open callbacks                                                */
/* ------------------------------------------------------------------ */

static struct dentry *hw_create_buf_file(const char *filename,
					 struct dentry *parent,
					 umode_t mode,
					 struct rchan_buf *buf,
					 int *is_global)
{
	/*
	 * Use a single global buffer so that events from all CPUs
	 * land in one file.  This simplifies userspace reading and
	 * avoids ordering puzzles in the log stream.
	 */
	*is_global = 1;
	return debugfs_create_file(filename, mode, parent, buf,
				   &relay_file_operations);
}

static int hw_remove_buf_file(struct dentry *dentry)
{
	debugfs_remove(dentry);
	return 0;
}

static const struct rchan_callbacks hw_relay_cbs = {
	.create_buf_file = hw_create_buf_file,
	.remove_buf_file = hw_remove_buf_file,
	/* .subbuf_start is optional — not needed for write-only logging. */
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int hw_relay_init(struct hw_state *state)
{
	hw_debug_dir = debugfs_create_dir("hook_write", NULL);
	if (IS_ERR_OR_NULL(hw_debug_dir)) {
		pr_err("failed to create debugfs directory\n");
		hw_debug_dir = NULL;
		return -ENOMEM;
	}

	state->relay_channel = relay_open("events", hw_debug_dir,
					  262144, 8,
					  &hw_relay_cbs, NULL);
	if (!state->relay_channel) {
		pr_err("failed to create relay channel\n");
		debugfs_remove(hw_debug_dir);
		hw_debug_dir = NULL;
		return -ENOMEM;
	}

	pr_info("relay channel created at debugfs/hook_write/events "
		"(subbuf_size=262144, n_subbufs=8)\n");
	return 0;
}

void hw_relay_write_event(struct syscall_event *ev)
{
	struct rchan *chan;

	if (unlikely(!ev))
		return;

	chan = hw_state.relay_channel;
	if (unlikely(!chan))
		return;

	relay_write(chan, ev, sizeof(*ev));
}

void hw_relay_exit(struct hw_state *state)
{
	if (state->relay_channel) {
		relay_close(state->relay_channel);
		state->relay_channel = NULL;
	}

	if (hw_debug_dir) {
		debugfs_remove(hw_debug_dir);
		hw_debug_dir = NULL;
	}
}
