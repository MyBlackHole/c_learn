/*
 * relay_reader.c - userspace relayfs event reader for hook_write
 *
 * Reads binary syscall_event records from the kernel module's relayfs
 * channel at /sys/kernel/debug/hook_write/events (or events0).
 *
 * Usage:
 *   ./relay_reader                       # continuous, follow mode
 *   ./relay_reader -1                    # one-shot: dump & exit
 *   ./relay_reader -p /sys/.../events0  # custom path
 *
 * Build:
 *   gcc -std=gnu11 -O2 -Wall -Wextra -o relay_reader relay_reader.c
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <getopt.h>

/* ------------------------------------------------------------------ */
/* Must match include/events.h exactly                                 */
/* ------------------------------------------------------------------ */

#define HW_PATH_MAX     256
#define TASK_COMM_LEN   16

struct syscall_event {
    uint64_t    timestamp_ns;
    uint32_t    pid;
    uint32_t    tid;
    uint32_t    syscall_nr;
    uint32_t    fd;
    uint64_t    count;
    uint64_t    offset;
    char        comm[TASK_COMM_LEN];    /* 16 bytes */
    char        path[HW_PATH_MAX];      /* 256 bytes */
} __attribute__((packed));

#define EVENT_SZ        sizeof(struct syscall_event)
#define SUBBUF_SIZE     262144
#define N_SUBBUFS       8

/* Must match kernel side exactly */
_Static_assert(EVENT_SZ == 312,
               "struct syscall_event size mismatch (expected 312)");

/* ------------------------------------------------------------------ */
/* Syscall name table                                                  */
/* ------------------------------------------------------------------ */

struct nr_name {
    int nr;
    const char *name;
};

static const struct nr_name nr_table[] = {
    {   1, "write"     },
    {  18, "pwrite64"  },
    {  20, "writev"    },
    { 296, "pwritev"   },
    { 328, "pwritev2"  },
    {  -1, NULL        },
};

static const char *nr_to_name(unsigned int nr)
{
    for (const struct nr_name *p = nr_table; p->name; p++)
        if (p->nr == (int)nr)
            return p->name;
    return "?";
}

/* ------------------------------------------------------------------ */
/* Timestamp helpers                                                   */
/* ------------------------------------------------------------------ */

static void format_ts(char *buf, size_t bufsz, uint64_t ns)
{
    struct timespec ts;
    struct tm tm;
    time_t sec;
    int rem_us;

    ts.tv_sec  = (time_t)(ns / 1000000000ULL);
    ts.tv_nsec = (long)(ns % 1000000000ULL);
    sec  = ts.tv_sec;
    rem_us = (int)(ts.tv_nsec / 1000);

    localtime_r(&sec, &tm);
    strftime(buf, bufsz, "%H:%M:%S", &tm);

    size_t len = strlen(buf);
    if (len + 8 < bufsz)
        snprintf(buf + len, bufsz - len, ".%06d", rem_us);
}

/* ------------------------------------------------------------------ */
/* Event parsing & printing                                            */
/* ------------------------------------------------------------------ */

static int parse_events(const unsigned char *data, int datalen, FILE *out)
{
    int n = 0;
    int off = 0;

    while (off + (int)EVENT_SZ <= datalen) {
        const struct syscall_event *ev =
            (const struct syscall_event *)(data + off);
        char ts[64];

        format_ts(ts, sizeof(ts), ev->timestamp_ns);

        /* Ensure null-terminated strings (safety even if kernel side is buggy) */
        char comm[TASK_COMM_LEN + 1];
        char path[HW_PATH_MAX + 1];
        memcpy(comm, ev->comm, TASK_COMM_LEN);
        comm[TASK_COMM_LEN] = '\0';
        memcpy(path, ev->path, HW_PATH_MAX);
        path[HW_PATH_MAX] = '\0';

        /* Filter out zeroed / uninitialized events */
        if (ev->timestamp_ns == 0)
            break;

        fprintf(out, "%-15s %-6u %-6u %-16s %-3d %-8s fd=%-3d "
                     "count=%-8lu offset=%-8lu\n",
                ts,
                ev->pid,
                ev->tid,
                comm,
                ev->fd,
                nr_to_name(ev->syscall_nr),
                ev->fd,
                (unsigned long)ev->count,
                (unsigned long)ev->offset);

        n++;
        off += EVENT_SZ;
    }

    return n;
}

/* ------------------------------------------------------------------ */
/* Relay file reading                                                  */
/* ------------------------------------------------------------------ */

static int try_open(const char **paths)
{
    for (int i = 0; paths[i]; i++) {
        int fd = open(paths[i], O_RDONLY);
        if (fd >= 0) {
            fprintf(stderr, "Opened %s\n", paths[i]);
            return fd;
        }
    }
    return -1;
}

static const char *default_paths[] = {
    "/sys/kernel/debug/hook_write/events0",
    "/sys/kernel/debug/hook_write/events",
    NULL,
};

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  -p PATH   relayfs file path (default: auto-detect)\n"
            "  -1        one-shot mode (dump and exit)\n"
            "  -h        show this help\n",
            prog);
}

int main(int argc, char **argv)
{
    const char *user_path = NULL;
    bool oneshot = false;
    int opt;

    while ((opt = getopt(argc, argv, "p:1h")) != -1) {
        switch (opt) {
        case 'p': user_path = optarg; break;
        case '1': oneshot = true;     break;
        case 'h': usage(argv[0]);     return 0;
        default:  usage(argv[0]);     return 1;
        }
    }

    /* Open relay file */
    const char **paths;
    const char *single[2];
    if (user_path) {
        single[0] = user_path;
        single[1] = NULL;
        paths = single;
    } else {
        paths = default_paths;
    }

    int fd = try_open(paths);
    if (fd < 0) {
        fprintf(stderr, "ERROR: cannot open relay file "
                        "(is hook_write.ko loaded?)\n");
        return 1;
    }

    /* Allocate read buffer (one sub-buffer) */
    unsigned char *buf = malloc(SUBBUF_SIZE);
    if (!buf) {
        perror("malloc");
        close(fd);
        return 1;
    }

    fprintf(stderr, "Reading relay events... (Ctrl+C to stop)\n"
                    "%-15s %-6s %-6s %-16s %-3s %-8s %s\n",
            "TIMESTAMP", "PID", "TID", "COMM", "FD", "SYSCALL",
            "DETAILS");

    if (oneshot) {
        /* One-shot: read whatever is available */
        int nread = read(fd, buf, SUBBUF_SIZE);
        if (nread > 0) {
            int n = parse_events(buf, nread, stdout);
            fprintf(stderr, "Read %d events (%d bytes)\n", n, nread);
        } else if (nread == 0) {
            fprintf(stderr, "No events available\n");
        } else {
            perror("read");
        }
    } else {
        /* Continuous: poll + read loop */
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int total = 0;

        while (1) {
            int ret = poll(&pfd, 1, -1);
            if (ret < 0) {
                if (errno == EINTR)
                    continue;
                perror("poll");
                break;
            }

            if (pfd.revents & POLLIN) {
                int nread = read(fd, buf, SUBBUF_SIZE);
                if (nread > 0) {
                    int n = parse_events(buf, nread, stdout);
                    total += n;
                    fflush(stdout);
                } else if (nread < 0 && errno != EAGAIN) {
                    perror("read");
                    break;
                }
            }

            if (pfd.revents & (POLLHUP | POLLERR)) {
                fprintf(stderr, "relay file hung up (module unloaded?)\n");
                break;
            }
        }

        fprintf(stderr, "Total events read: %d\n", total);
    }

    free(buf);
    close(fd);
    return 0;
}
