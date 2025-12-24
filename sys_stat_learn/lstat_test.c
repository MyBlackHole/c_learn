#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int demo_lstat_main(int argc, char *argv[])
{
	struct stat buf;
	int ret;
	ret = lstat(argv[1], &buf);
	if (ret != 0) {
		perror("lstat");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
