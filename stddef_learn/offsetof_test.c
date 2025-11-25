#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// #define offsetof(type, member) (int)(&((type *)0)->member)

// // 偏移量
// #define offsetof(type, member) __builtin_offsetof(type, member)

typedef struct test {
	char i;
	int j;
	char k;
} test_t;
//

static void test1()
{
	test_t *ptr = malloc(sizeof(test_t));
	ptr->j = 100;
	printf("%ld -- %p -- %d\n", offsetof(test_t, j), ptr,
	       *(int *)((size_t)ptr + offsetof(test_t, j)));
}
struct address {
	char name[50];
	char street[50];
	int phone;
};

static void test2()
{
	printf("address 结构中的 name 偏移 = %lu 字节。\n",
	       offsetof(struct address, name));

	printf("address 结构中的 street 偏移 = %lu 字节。\n",
	       offsetof(struct address, street));

	printf("address 结构中的 phone 偏移 = %lu 字节。\n",
	       offsetof(struct address, phone));
}

int main(int argc, char *argv[])
{
	test1();
	test2();
	return EXIT_SUCCESS;
}


// ❯ xmake run stddef_learn
// 4 -- 0x558f8cdf0320 -- 100
// address 结构中的 name 偏移 = 0 字节。
// address 结构中的 street 偏移 = 50 字节。
// address 结构中的 phone 偏移 = 100 字节。
