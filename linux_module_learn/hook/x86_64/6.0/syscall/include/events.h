/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * events.h - syscall event record definition
 *
 * Copyright (C) 2026 hook_system_call contributors
 *
 * Defines the fixed-size binary record written into the relayfs
 * channel for every hooked syscall invocation.
 */

#ifndef _HW_EVENTS_H
#define _HW_EVENTS_H

#include <linux/types.h>
#include <linux/sched.h>

/* Re-export the path-length constant for convenience. */
#define HW_PATH_MAX	256

/**
 * struct syscall_event - binary record for one hooked syscall.
 *
 * Every field is fixed-size to simplify userspace parsing from the
 * relayfs mmap buffer.  The struct is packed to avoid implicit
 * padding and ensure a stable ABI across compiler versions.
 *
 * Field layout (312 bytes total):
 *   offset  size  field
 *   0       8     timestamp_ns
 *   8       4     pid
 *   12      4     tid
 *   16      4     syscall_nr
 *   20      4     fd
 *   24      8     count
 *   32      8     offset
 *   40      16    comm
 *   56      256   path
 */
struct syscall_event {
	u64	timestamp_ns;
	u32	pid;
	u32	tid;
	u32	syscall_nr;
	u32	fd;
	u64	count;
	u64	offset;
	char	comm[TASK_COMM_LEN];	/* 16 bytes */
	char	path[HW_PATH_MAX];	/* 256 bytes */
} __packed;

/* Total size of a syscall_event record (must match the layout above). */
#define HW_EVENT_SIZE		sizeof(struct syscall_event)

#endif /* _HW_EVENTS_H */
