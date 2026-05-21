#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <liburing.h>

#define FILE_PATH "./testfile"
#define OLD_DATA   "old data from previous write\n"
#define NEW_DATA   "new data from async io_uring write\n"

/* 使用 io_uring 异步写入数据到文件指定偏移 */
int async_write_data(const char *file, const char *data, size_t len, off_t offset) {
    struct io_uring ring;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    int fd, ret;

    /* 打开文件（普通缓冲 I/O，无 O_SYNC/O_DIRECT） */
    fd = open(file, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    /* 初始化 io_uring 队列（默认大小 8 就够） */
    ret = io_uring_queue_init(8, &ring, 0);
    if (ret) {
        fprintf(stderr, "io_uring_queue_init: %s\n", strerror(-ret));
        close(fd);
        return -1;
    }

    /* 获取一个 SQE */
    sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fprintf(stderr, "io_uring_get_sqe failed\n");
        io_uring_queue_exit(&ring);
        close(fd);
        return -1;
    }

    /* 准备异步写操作 */
    io_uring_prep_write(sqe, fd, data, len, offset);
    /* 不设置任何特殊标志，使用默认缓冲 I/O */

    /* 提交写请求 */
    ret = io_uring_submit(&ring);
    if (ret < 0) {
        fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
        io_uring_queue_exit(&ring);
        close(fd);
        return -1;
    }

    /* 等待完成（得到 CQE） */
    ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
        fprintf(stderr, "io_uring_wait_cqe: %s\n", strerror(-ret));
        io_uring_queue_exit(&ring);
        close(fd);
        return -1;
    }

    /* 检查写操作是否成功 */
    if (cqe->res < 0) {
        fprintf(stderr, "async write failed: %s\n", strerror(-cqe->res));
        io_uring_cqe_seen(&ring, cqe);
        io_uring_queue_exit(&ring);
        close(fd);
        return -1;
    }

    printf("[Writer] Async write succeeded (size=%d, offset=%ld)\n", cqe->res, (long)offset);
    io_uring_cqe_seen(&ring, cqe);

    /* 注意：这里没有调用 fsync / fdatasync / close 也不会自动落盘 */
    io_uring_queue_exit(&ring);
    close(fd);   /* 关闭文件描述符不会刷新页缓存 */
    return 0;
}

/* 普通读取文件指定偏移的内容 */
void read_file(const char *file, off_t offset, size_t len) {
    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        perror("open for read");
        return;
    }

    char *buf = malloc(len + 1);
    if (!buf) {
        perror("malloc");
        close(fd);
        return;
    }

    ssize_t n = pread(fd, buf, len, offset);
    if (n < 0) {
        perror("pread");
        free(buf);
        close(fd);
        return;
    }

    buf[n] = '\0';
    printf("[Reader] Read content: %s", buf);
    free(buf);
    close(fd);
}

int demo_test_async_write_main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [prepare|write|read]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "prepare") == 0) {
        /* 1. 准备旧数据 */
        int fd = open(FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("prepare open");
            return 1;
        }
        write(fd, OLD_DATA, strlen(OLD_DATA));
        close(fd);
        printf("[Prepare] Wrote old data: %s", OLD_DATA);
        return 0;
    }
    else if (strcmp(argv[1], "write") == 0) {
        /* 2. 进程 A: 异步写入新数据，不落盘 */
        return async_write_data(FILE_PATH, NEW_DATA, strlen(NEW_DATA), 0);
    }
    else if (strcmp(argv[1], "read") == 0) {
        /* 3. 进程 B: 读取同一位置 */
        read_file(FILE_PATH, 0, strlen(NEW_DATA));
        return 0;
    }
    else {
        fprintf(stderr, "Unknown command\n");
        return 1;
    }
}
