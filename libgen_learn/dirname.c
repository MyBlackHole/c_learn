#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
	char path_tmp[PATH_MAX] = {};
	char *dir_path = NULL;
	char *path_ptr = "/bin/xxxx/";

	memcpy(path_tmp, path_ptr, strlen(path_ptr));
	dir_path = dirname(path_tmp);
	printf("%s\n", dir_path);

	path_ptr = "/bin/xxxx";
	memcpy(path_tmp, path_ptr, strlen(path_ptr));
	dir_path = dirname(path_tmp);
	printf("%s\n", dir_path);

	path_ptr = "bin/xxxx";
	memcpy(path_tmp, path_ptr, strlen(path_ptr));
	dir_path = dirname(path_tmp);
	printf("%s\n", dir_path);

	return EXIT_SUCCESS;
}
