#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>

#define THREAD_NUM 32

volatile unsigned int sum = 0;
unsigned int max = 10000000;
volatile bool cond = true;

typedef int slock_t;

static __inline__ int cas(volatile slock_t *lock)
{
	slock_t expected = 0;
	return !(__atomic_compare_exchange_n(lock, &expected, (slock_t)1, false,
					     __ATOMIC_ACQUIRE,
					     __ATOMIC_ACQUIRE));
}

#define S_UNLOCK(lock) __atomic_store_n(lock, (slock_t)0, __ATOMIC_RELEASE)
#define S_INIT_LOCK(lock) S_UNLOCK(lock)
#define SPIN(lock) (*(lock) ? 1 : cas(lock))

volatile slock_t g_lock;

int s_lock(volatile slock_t *lock)
{
	int i = 0;

	while (SPIN(lock)) {
		++i;
		sched_yield();
	}

	return i;
}

void s_unlock(volatile slock_t *lock)
{
	S_UNLOCK(lock);
}

void *thread_func(void *args)
{
	int ret = 0;
	pthread_t self = pthread_self();
	printf("thread %lu started\n", (unsigned long)self);
	while ((cond = sum < max)) {
		ret = s_lock(&g_lock);
		++sum;
		printf("thread %lu lock %d sum %d\n", (unsigned long)self, ret,
		       sum);
		s_unlock(&g_lock);
	}

	return NULL;
}

int demo___atomic_compare_exchange_n_main(void)
{
	pthread_t pids[THREAD_NUM];
	int i, ret;
	void *val;

	S_INIT_LOCK(&g_lock);

	for (i = 0; i < THREAD_NUM; ++i) {
		ret = pthread_create(&pids[i], NULL, thread_func, NULL);
		if (ret) {
			printf("pthread_create failed: %d\n", ret);
			return EXIT_FAILURE;
		}
	}

	for (i = 0; i < THREAD_NUM; ++i) {
		pthread_join(pids[i], &val);
	}

	printf("final sum: %d\n", sum);
	return EXIT_SUCCESS;
}
