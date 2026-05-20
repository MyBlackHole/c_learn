#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libaio.h>

// 必须与文件系统的 block size 对齐，通常为 4096 字节
#define BUFFER_SIZE 4096

int main()
{
	// 1. 打开文件（使用 O_DIRECT 绕过页缓存，libaio 通常要求此标志）
	int fd = open("testfile", O_RDONLY | O_DIRECT);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	// 2. 分配对齐内存（O_DIRECT 要求内存地址与块大小对齐）
	void *buffer;
	if (posix_memalign(&buffer, BUFFER_SIZE, BUFFER_SIZE) != 0) {
		perror("posix_memalign");
		close(fd);
		return 1;
	}
	memset(buffer, 0, BUFFER_SIZE);

	// 3. 初始化异步 I/O 上下文
	io_context_t ctx = 0;
	if (io_setup(1, &ctx) != 0) {
		perror("io_setup");
		free(buffer);
		close(fd);
		return 1;
	}

	// 4. 准备 iocb（I/O 控制块）
	struct iocb cb; // 注意是 iocb，不是 ioccb
	struct iocb *cbs[1] = { &cb };
	io_prep_pread(&cb, fd, buffer, BUFFER_SIZE, 0);

	// 5. 提交异步 I/O 请求
	if (io_submit(ctx, 1, cbs) != 1) {
		perror("io_submit");
		io_destroy(ctx);
		free(buffer);
		close(fd);
		return 1;
	}

	// 6. 等待 I/O 操作完成
	struct io_event events[1];
	if (io_getevents(ctx, 1, 1, events, NULL) != 1) {
		perror("io_getevents");
		io_destroy(ctx);
		free(buffer);
		close(fd);
		return 1;
	}

	// events[0].res 表示实际读取的字节数（成功时），负数表示错误码
	long bytes_read = events[0].res;
	if (bytes_read < 0) {
		fprintf(stderr, "read error: %s\n", strerror(-bytes_read));
	} else {
		printf("Read %ld bytes: %.*s\n", bytes_read, (int)bytes_read,
		       (char *)buffer);
	}

	// 7. 清理资源
	io_destroy(ctx);
	free(buffer);
	close(fd);

	return 0;
}
