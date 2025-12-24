#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int mkdir_path(const char *path)
{
	if (path == NULL || *path == '\0') {
		errno = EINVAL;
		return -1;
	}

	if (mkdirat(AT_FDCWD, path, 0755) == 0 || errno == EEXIST) {
		return 0;
	}

	if (errno != ENOENT) {
		return -1;
	}

	char *p = strdup(path);
	if (p == NULL) {
		return -1;
	}

	int dfd = AT_FDCWD;
	char *start = p;
	char *end;

	int using_rootfd = 0;
	if (*start == '/') {
		dfd = open("/", O_PATH | O_DIRECTORY);
		if (dfd < 0) {
			free(p);
			return -1;
		}
		using_rootfd = 1;
		start++;
	}

	while ((end = strchr(start, '/')) != NULL) {
		char saved_char = *end;
		*end = '\0';

		if (mkdirat(dfd, start, 0755) != 0 && errno != EEXIST) {
			*end = saved_char;
			goto cleanup_fail;
		}

		int new_fd = openat(dfd, start, O_PATH | O_DIRECTORY);
		*end = saved_char;

		if (new_fd < 0) {
			goto cleanup_fail;
		}

		if (using_rootfd || dfd != AT_FDCWD) {
			close(dfd);
		}
		dfd = new_fd;
		using_rootfd = 0;
		start = end + 1;
	}

	if (strlen(start) > 0 && mkdirat(dfd, start, 0755) != 0 &&
	    errno != EEXIST) {
		goto cleanup_fail;
	}

	if (using_rootfd || dfd != AT_FDCWD) {
		close(dfd);
	}
	free(p);
	return 0;

cleanup_fail:
	if (using_rootfd || dfd != AT_FDCWD) {
		close(dfd);
	}
	free(p);
	return -1;
}

int demo_mkdirat_main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: %s <path>\n", argv[0]);
		return EXIT_FAILURE;
	}
	return mkdir_path(argv[1]);
}
