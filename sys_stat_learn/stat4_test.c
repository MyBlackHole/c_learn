#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef SOLARIS
#include <sys/mkdev.h>
#endif
#include <sys/sysmacros.h>

int demo_stat4_main(int argc, char **argv)
{
	printf("demo_stat4_main\n");
	printf("%ld\n", sizeof(struct stat));
	exit(0);
}


// ❯ xmake run sys_stat_learn stat4
// demo_stat4_main
// 144
