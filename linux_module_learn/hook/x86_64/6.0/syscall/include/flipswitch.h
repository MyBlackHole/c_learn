/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * flipswitch.h - FlipSwitch syscall dispatch patching API
 *
 * Copyright (C) 2026 hook_system_call contributors
 *
 * Post-6.9 syscall hooking via x64_sys_call() call instruction patching.
 * On Linux 6.9+, the syscall dispatcher uses an inlined switch-statement
 * instead of the traditional sys_call_table array lookup.  FlipSwitch
 * locates the 'call rel32' instruction that dispatches a given syscall
 * inside x64_sys_call() and patches the relative offset to redirect
 * execution through a user-supplied hook function.
 */

#ifndef _HW_FLIPSWITCH_H
#define _HW_FLIPSWITCH_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/relay.h>

/* Forward declaration (defined in events.h). */
struct syscall_event;

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

/**
 * HW_MAX_HOOKS - maximum number of concurrently-hooked syscalls.
 * We currently target 4 write-family syscalls; space for 8 leaves
 * room for future expansion (read, open, etc.) without recompiling.
 */
#define HW_MAX_HOOKS		8

/**
 * HW_SCAN_SIZE - how many bytes of x64_sys_call() to scan for 'call'
 * instructions.  The function contains ~450 cases; each case compiles
 * to ~8-12 bytes on average (~4 KB total). 8192 is conservative.
 */
#define HW_SCAN_SIZE		8192

/**
 * HW_PATH_MAX - maximum length of a resolved file path stored in an
 * event record.  PATH_MAX (4096) is large; 256 is practical for the
 * vast majority of real-world paths.
 */
#define HW_PATH_MAX		256

/* Hook-enable bit positions for the module parameter hook_mask. */
#define HW_BIT_WRITE		0
#define HW_BIT_WRITEV		1
#define HW_BIT_PWRITE64		2
#define HW_BIT_PWRITEV2		3

#define HW_MASK_WRITE		BIT(HW_BIT_WRITE)
#define HW_MASK_WRITEV		BIT(HW_BIT_WRITEV)
#define HW_MASK_PWRITE64	BIT(HW_BIT_PWRITE64)
#define HW_MASK_PWRITEV2	BIT(HW_BIT_PWRITEV2)
#define HW_MASK_ALL		(0x0FU)

/* ------------------------------------------------------------------ */
/* Type definitions                                                   */
/* ------------------------------------------------------------------ */

/**
 * struct hw_patch - state for one patched 'call' instruction.
 * @call_addr:	   virtual address of the 0xE8 instruction inside
 *		   x64_sys_call().
 * @orig_offset:   original signed 32-bit relative offset that was
 *		   stored at call_addr + 1 (used for restoration).
 * @applied:	   true while the patch is active.
 */
struct hw_patch {
	void		*call_addr;
	int32_t		 orig_offset;
	bool		 applied;
};

/**
 * struct hw_hook - registration record for one hooked syscall.
 * @syscall_nr:	syscall number (e.g. __NR_write).
 * @hook_fn:	replacement function (must match syscall ABI).
 * @orig_fn:	address of the original syscall function, read from
 *		sys_call_table before patching.
 * @patch:		FlipSwitch patch state.
 * @enabled:	true if the patch is currently applied.
 * @name:		human-readable name for debugging / logging.
 */
struct hw_hook {
	unsigned int	 syscall_nr;
	long		(*hook_fn)(struct pt_regs *);
	unsigned long	 orig_fn;
	struct hw_patch	 patch;
	bool		 enabled;
	char		 name[32];
};

/**
 * struct hw_state - top-level module state.
 *
 * All mutable fields are serialised by @lock except for @in_flight
 * (atomic) and @unloading (marked before synchronize_rcu()).
 */
struct hw_state {
	/* Symbols resolved at init ******************************** */
	unsigned long	 x64_sys_call_addr;
	unsigned long	 sys_call_table_addr;

	/* Registered hooks **************************************** */
	struct hw_hook	 hooks[HW_MAX_HOOKS];
	int		 nr_hooks;

	/* Control-path lock *************************************** */
	struct mutex	 lock;

	/* Concurrency safety ************************************** */
	atomic_t	 in_flight;
	bool		 unloading;

	/* Data relay ********************************************** */
	struct rchan	*relay_channel;
};

/* ------------------------------------------------------------------ */
/* Core API (flipswitch_core.c)                                       */
/* ------------------------------------------------------------------ */

/**
 * hw_find_symbols() - resolve x64_sys_call and sys_call_table addresses.
 * @state: module state to fill in.
 *
 * Uses kallsyms_on_each_symbol() (EXPORT_SYMBOL_GPL) to iterate the
 * kernel symbol table.  Both symbols are required for FlipSwitch to
 * operate.
 *
 * Return: 0 on success, -ENXIO if either symbol could not be found.
 */
int hw_find_symbols(struct hw_state *state);

/**
 * hw_scan_x64_sys_call() - locate the 'call rel32' for a given syscall.
 * @state:  module state (must have x64_sys_call_addr filled in).
 * @target: address of the original syscall function to find.
 *
 * Scans the first HW_SCAN_SIZE bytes of x64_sys_call() for 0xE8
 * (call rel32) instructions.  For each match the target address is
 * computed as (call_addr + 5 + rel32); if it matches @target the
 * call site is returned.
 *
 * Return: kernel virtual address of the matching call instruction,
 *	   or NULL if no match is found.
 */
void *hw_scan_x64_sys_call(struct hw_state *state, unsigned long target);

/**
 * hw_apply_patch() - apply a FlipSwitch patch.
 * @patch: patch descriptor.  call_addr and orig_offset must be valid.
 * @hook_fn:  address of the replacement function.
 *
 * Disables CR0.WP, writes the new relative offset (calculated so that
 * the call targets @hook_fn), flushes the write-buffer, re-enables WP,
 * and serialises the instruction stream.
 */
void hw_apply_patch(struct hw_patch *patch, void *hook_fn);

/**
 * hw_restore_patch() - restore a previously-applied FlipSwitch patch.
 * @patch: patch descriptor to restore.
 *
 * Writes the original relative offset back, with the same CR0.WP /
 * serialisation dance as hw_apply_patch().
 */
void hw_restore_patch(struct hw_patch *patch);

/* ------------------------------------------------------------------ */
/* Hook registration (hook_core.c)                                     */
/* ------------------------------------------------------------------ */

/**
 * hw_register_hooks() - look up syscall addresses and apply patches.
 * @state: module state with resolved symbols.
 *
 * For each syscall in state->hooks[]:
 *   1. read the original function address from sys_call_table[]
 *   2. locate the corresponding call instruction in x64_sys_call()
 *   3. save the original offset and apply the patch
 *
 * Return: 0 on success, negative errno on failure.
 */
int hw_register_hooks(struct hw_state *state);

/**
 * hw_unregister_hooks() - restore all patches and clean up.
 * @state: module state.
 *
 * Restores every active patch, then zeroes the hook array so that
 * stale function pointers cannot be used after module exit.
 */
void hw_unregister_hooks(struct hw_state *state);

/* ------------------------------------------------------------------ */
/* Hook registration (hook_write.c)                                    */
/* ------------------------------------------------------------------ */

int  hw_init_write_hooks(struct hw_state *state);
void hw_sync_orig_ptrs(struct hw_state *state);
void hw_exit_write_hooks(struct hw_state *state);

/* ------------------------------------------------------------------ */
/* Path resolution (path_resolve.c)                                    */
/* ------------------------------------------------------------------ */

int hw_resolve_fd_path(unsigned int fd, char *buf, size_t bufsz);

/* ------------------------------------------------------------------ */
/* Relay channel (relay_chan.c)                                        */
/* ------------------------------------------------------------------ */

int  hw_relay_init(struct hw_state *state);
void hw_relay_write_event(struct syscall_event *ev);
void hw_relay_exit(struct hw_state *state);

/* ------------------------------------------------------------------ */
/* Control (control.c)                                                 */
/* ------------------------------------------------------------------ */

void hw_control_init(struct hw_state *state);
void hw_control_exit(struct hw_state *state);
bool hw_control_is_enabled(unsigned int syscall_nr);

/* ------------------------------------------------------------------ */
/* Security (security.c)                                               */
/* ------------------------------------------------------------------ */

void hw_security_event_begin(void);
void hw_security_event_end(void);
void hw_security_init(struct hw_state *state);
void hw_security_wait_drain(struct hw_state *state);
void hw_security_exit(struct hw_state *state);

/* ------------------------------------------------------------------ */
/* CRC / build sanity                                                 */
/* ------------------------------------------------------------------ */

/**
 * hw_assert_build() - compile-time assertions.
 *
 * Called once during module_init() to verify that the event record
 * layout is as expected.  Panics the module on mismatch so that
 * userspace parsers are never confused by a different layout.
 */
void hw_assert_build(void);

#endif /* _HW_FLIPSWITCH_H */
