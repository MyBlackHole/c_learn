#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int demo_self_exe_main(int argc, char *argv[])
{
	char path[PATH_MAX] = {};
	int rc = readlink("/proc/self/exe", path, PATH_MAX);
	if (rc < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	path[rc] = '\0';
	fprintf(stdout, "%s\n", path);
	return EXIT_SUCCESS;
}
