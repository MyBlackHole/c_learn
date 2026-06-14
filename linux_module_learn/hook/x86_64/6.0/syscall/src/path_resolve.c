// SPDX-License-Identifier: GPL-2.0-only
/*
 * path_resolve.c - resolve fd to a canonical file path
 *
 * Uses fget() + d_path() to translate a file descriptor into a
 * human-readable path string.  Falls back to "<unknown>" on error.
 */

#define pr_fmt(fmt) "hw: " fmt

#include <linux/kernel.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/mount.h>
#include <linux/sched.h>
#include <linux/string.h>

#include "flipswitch.h"

int hw_resolve_fd_path(unsigned int fd, char *buf, size_t bufsz)
{
	struct file *file;
	char *path;
	int ret;

	if (!buf || bufsz < 1)
		return -EINVAL;

	file = fget(fd);
	if (!file) {
		strscpy(buf, "<unknown>", bufsz);
		return -EBADF;
	}

	path = d_path(&file->f_path, buf, bufsz);
	if (IS_ERR(path)) {
		ret = PTR_ERR(path);
		strscpy(buf, "<error>", bufsz);
		goto out;
	}

	/*
	 * d_path() fills the buffer from the end, so @path may point
	 * somewhere in the middle of @buf.  Move the string to the
	 * beginning if necessary.
	 */
	if (path != buf) {
		size_t len = strnlen(path, bufsz - 1);

		memmove(buf, path, len);
		buf[len] = '\0';
	}

	ret = 0;

out:
	fput(file);
	return ret;
}
