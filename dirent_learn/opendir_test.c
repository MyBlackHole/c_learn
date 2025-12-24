#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int demo_opendir_test_main(int argc, char *argv[])
{
	DIR *dir;
	struct dirent *dirp;

	if (argc != 2) {
		perror("usage: ls directory_name");
		exit(1);
	}
	dir = opendir(argv[1]);
	if (dir == NULL) {
		fprintf(stderr, "can't open %s\n", argv[1]);
		fprintf(stderr, "err %s, id %d\n", strerror(errno), errno);
		exit(1);
	}
	while ((dirp = readdir(dir)) != NULL) {
		fprintf(stderr, "%s\n", dirp->d_name);
	}

	closedir(dir);
	exit(0);
}

// ❯ xmake run dirent_learn opendir_test /boot
// .
// ..
// vmlinuz-linux
// initramfs-linux.img
// initramfs-linux-fallback.img
// grub
// EFI
//
//
//
// ❯ xmake run dirent_learn opendir_test /tmp/ccAhreOD.s
// can't open /tmp/ccAhreOD.s
// err Not a directory, id 20
// error: execv(/run/media/black/Data/Documents/c/build/linux/x86_64/debug/dirent_learn opendir_test /tmp/ccAhreOD.s) failed(1)
