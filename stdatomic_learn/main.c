#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS 10
#define INCREMENTS_PER_THREAD 100000

atomic_uint_fast64_t counter = ATOMIC_VAR_INIT(0);

void *thread_func(void *arg)
{
	for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
		atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed);
	}
	return NULL;
}

int main()
{
	pthread_t threads[NUM_THREADS];

	// 创建线程
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, thread_func, NULL);
	}

	// 等待所有线程完成
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	printf("Expected: %d\n", NUM_THREADS * INCREMENTS_PER_THREAD);
	printf("Actual: %lu\n", atomic_load(&counter));

	return 0;
}
