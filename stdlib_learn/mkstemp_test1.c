#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int demo_mkstemp1_main()
{
	char template[] = "/tmp/XXXXXX";
	int fd = mkstemp(template);
	if (fd == -1) {
		perror("mkstemp");
		exit(1);
	}

	printf("%s", template);

	// 立即删除文件，但文件描述符仍然有效
	unlink(template); // 文件从目录中删除，但数据仍在直到 fd 关闭

	// 对 fd 进行读写操作...
	write(fd, "hello", 5);

	// 关闭 fd 后文件真正被删除
	close(fd);

	return 0;
}
