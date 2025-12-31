#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int demo_F_GETLK_main(int argc, char **argv)
{
	int fd, ret;
	struct flock lock;
	if (argc != 2) {
		printf("Usage: %s <file>\n", argv[0]);
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	memset(&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_len = 0;
	ret = fcntl(fd, F_GETLK, &lock);
	if (ret < 0) {
		perror("fcntl");
		close(fd);
		return EXIT_FAILURE;
	}
	printf("F_GETLK: type=%d, whence=%d, start=%ld, len=%ld, pid=%d\n",
	       lock.l_type, lock.l_whence, lock.l_start, lock.l_len,
	       lock.l_pid);
	close(fd);
	return EXIT_SUCCESS;
}

// ❯ xmake run fcntl_learn F_GETLK /opt/aio/airflow/tools/fs-tools/tmp/wdg.pid
// F_GETLK: type=1, whence=0, start=0, len=0, pid=625446
