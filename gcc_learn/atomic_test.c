#include <stdint.h>
#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS 10
#define INCREMENTS_PER_THREAD 100000

uint64_t counter = 0;

static void *thread_func(void *arg)
{
	for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
		__sync_fetch_and_add(&counter, 1);
	}
	return NULL;
}

// 使用现代内置函数的版本
static void *thread_func_modern(void *arg)
{
	for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
		__atomic_fetch_add(&counter, 1, __ATOMIC_RELAXED);
	}
	return NULL;
}

int demo_atomic_main()
{
	pthread_t threads[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, thread_func, NULL);
		// pthread_create(&threads[i], NULL, thread_func_modern, NULL);
		(void)thread_func_modern;
	}

	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	printf("Counter: %lu\n", counter);
	printf("Expected: %d\n", NUM_THREADS * INCREMENTS_PER_THREAD);

	return 0;
}
