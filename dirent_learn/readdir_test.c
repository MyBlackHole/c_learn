#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

int demo_readdir_test_main(int argc, char **argv)
{
	DIR *dir;
	struct dirent *dirp;

	if (argc < 2) {
		fprintf(stderr, "usage:ls directory_name");
		exit(1);
	}
	dir = opendir(argv[1]);
	if (dir == NULL) {
		perror("opendir");
		fprintf(stderr, "can't open %s\n", argv[1]);
		exit(1);
	}
	while ((dirp = readdir(dir)) != NULL) {
		printf("%s\n", dirp->d_name);
		if (dirp->d_type == DT_DIR) {
			printf("dir %s\n", dirp->d_name);
		} else if (dirp->d_type == DT_REG) {
			printf("file %s\n", dirp->d_name);
		} else if (dirp->d_type == DT_LNK) {
			printf("link %s\n", dirp->d_name);
		} else {
			printf("unknown %s\n", dirp->d_name);
		}
	}
	closedir(dir);
	exit(EXIT_SUCCESS);
}

// output:
//
// ❯ xmake run dirent_learn readdir_test Debug
// opendir: No such file or directory
// can't open Debug
// error: execv(/run/media/black/Data/Documents/c/build/linux/x86_64/debug/dirent_learn readdir_test Debug) failed(-1)
