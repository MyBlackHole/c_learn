#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

const int i = 5;
__thread int var = i; //两种方式效果一样
static __thread int var2 = 15;

static void *worker1(void *arg)
{
	printf("worker1 var :%d\n", ++var);
	printf("worker1 var addr :%p\n", &var);
	printf("worker1 var2 :%d\n", ++var2);
	printf("worker1 var2 addr :%p\n", &var2);
	return NULL;
}

static void *worker2(void *arg)
{
	sleep(1); //等待线程1改变var值，验证是否影响线程2
	printf("worker2 var :%d\n", --var);
	printf("worker2 var addr :%p\n", &var);
	printf("worker2 var2 :%d\n", --var2);
	printf("worker2 var2 addr :%p\n", &var2);
	return NULL;
}

int demo___thread_main()
{
	pthread_t pid1, pid2;
	static __thread int temp = 10; //修饰函数内的static变量

	pthread_create(&pid1, NULL, worker1, NULL);
	pthread_create(&pid2, NULL, worker2, NULL);
	pthread_join(pid1, NULL);
	pthread_join(pid2, NULL);

	printf("worker1 var addr :%p\n", &var);
	printf("worker1 var2 addr :%p\n", &var2);
	printf("worker1 temp addr :%p\n", &temp);
	return 0;
}
