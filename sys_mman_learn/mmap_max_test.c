#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#define NUM 10

// // success
// #define MAX_SIZE 100 * 1000 * 1000 * 1000 * 100
#define MAX_SIZE 100 * 1000 * 1000 * 1000 * 1000

typedef struct {
	char name[4];
	int age;
} people;

// map a normal file as shared mem:
int demo_mmap_max_main(int argc, char **argv)
{
	int myfd;
	people *p_map;
	int count = 0;
	char path[4096];

	while (count < NUM) {
		snprintf(path, sizeof(path), "/tmp/mmap_max_test_%d", count);
		// 以读写方式打开， 没有自动创建
		myfd = open(path, O_CREAT | O_RDWR,
			    S_IRWXU | S_IRWXG | S_IRWXO);
		if (myfd < 0) {
			perror(strerror(errno));
			exit(EXIT_FAILURE);
		}

		p_map = mmap(NULL, (size_t)MAX_SIZE, PROT_READ | PROT_WRITE,
			     MAP_SHARED, myfd, 0);
		if (p_map == MAP_FAILED) {
			perror(strerror(errno));
			exit(EXIT_FAILURE);
		}
		sleep(10);
		printf("p_map = %p\n", p_map);
		count++;
	}

	// // 解除映射时会把内存数据写回文件(后台达到条件也会写)，
	// // 但是不会超出文件大小
	// munmap(p_map, sizeof(people) * NUM);
	exit(EXIT_SUCCESS);
}
