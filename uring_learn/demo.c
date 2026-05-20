#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <liburing.h>

#define QUEUE_DEPTH 32 // 队列深度
#define MY_BLOCK_SIZE 4096 // 块大小，为方便示例，避免处理偏移量

int demo_demo_main()
{
	struct io_uring ring; // io_uring 实例
	struct io_uring_sqe *sqe; // 提交队列项 (Submission Queue Entry)
	struct io_uring_cqe *cqe; // 完成队列项 (Completion Queue Entry)
	int fd, ret;
	char *buf;

	// 1. 打开文件
	fd = open("example.txt", O_RDONLY);
	if (fd < 0) {
		perror("open");
		return -1;
	}

	// 2. 分配对齐的缓冲区 (O_DIRECT 强制要求)
	//    使用 posix_memalign 确保内存地址与块大小对齐
	if (posix_memalign((void **)&buf, MY_BLOCK_SIZE, MY_BLOCK_SIZE)) {
		perror("posix_memalign");
		close(fd);
		return -1;
	}
	memset(buf, 0, MY_BLOCK_SIZE);

	// 3. 初始化 io_uring 实例
	// 参数：队列深度，ring 结构体， flags (0 表示默认)
	ret = io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
	if (ret) {
		fprintf(stderr, "io_uring_queue_init failed: %s\n",
			strerror(-ret));
		free(buf);
		close(fd);
		return -1;
	}

	// 4. 获取一个 SQE 并准备读操作
	sqe = io_uring_get_sqe(&ring);
	if (!sqe) {
		fprintf(stderr, "Could not get SQE\n");
		io_uring_queue_exit(&ring);
		free(buf);
		close(fd);
		return -1;
	}
	// 准备一个普通的读操作
	// 参数：sqe, fd, 缓冲区地址, 读取大小, 文件偏移量
	io_uring_prep_read(sqe, fd, buf, MY_BLOCK_SIZE, 0);

	// 5. 提交请求
	ret = io_uring_submit(&ring);
	if (ret < 0) {
		fprintf(stderr, "io_uring_submit failed: %s\n", strerror(-ret));
		io_uring_queue_exit(&ring);
		free(buf);
		close(fd);
		return -1;
	}

	// 6. 等待请求完成 (阻塞)
	ret = io_uring_wait_cqe(&ring, &cqe);
	if (ret < 0) {
		fprintf(stderr, "io_uring_wait_cqe failed: %s\n",
			strerror(-ret));
		io_uring_queue_exit(&ring);
		free(buf);
		close(fd);
		return -1;
	}

	// 7. 处理结果: cqe->res 保存了操作结果 (类似 read 的返回值)
	if (cqe->res < 0) {
		fprintf(stderr, "Async read failed: %s\n", strerror(-cqe->res));
	} else {
		// cqe->res 是实际读取的字节数
		printf("成功读取 %d 字节，内容为: %.*s\n", cqe->res, cqe->res,
		       buf);
	}

	// 8. 标记该 CQE 已处理，以便内核回收
	io_uring_cqe_seen(&ring, cqe);

	// 9. 清理资源
	io_uring_queue_exit(&ring);
	free(buf);
	close(fd);
	return 0;
}
