#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include "mpmc_queue.h"

static size_t MAXSEQ = 10000000;
#define N 3 // 写线程数
#define M 1 // 读线程数
#define Q M

typedef struct {
	uint64_t a, b, c[61];
} entry_t;
volatile int n = 1, m = 1;

void *read_thread(void *arg)
{
	struct mpmc_queuebatch *buffer = (struct mpmc_queuebatch *)arg;
	struct mpmc_queuebatch_reader r[1];
	const entry_t *entry;

	size_t total = 0;
	struct timeval t[2];
	gettimeofday(&t[0], NULL);

	mpmc_queuebatch_reader_init(r, buffer);
	do {
		size_t n = 0, i;
		struct reader_result res;
		if ((n = mpmc_queuebatch_reader_prepare(r, &res)) == 0) {
			usleep(10);
			continue;
		}

		for (i = 0; i < n; i++) {
			entry = (entry_t *)mpmc_queuebatch_reader_next(&res);
			assert(entry != NULL);
			if (entry->a == UINT64_MAX)
				goto out;
			assert(entry->a + 1 == entry->b);
			total++;
		}

		mpmc_queuebatch_reader_commit(r, &res);
	} while (1);

out:
	gettimeofday(&t[1], NULL);
	printf("read time = %lf, total = %d\n",
	       (double)t[1].tv_sec * 1000000 + t[1].tv_usec -
		       (double)t[0].tv_sec * 1000000 - t[0].tv_usec,
	       (int)total);
	return NULL;
}

void *write_thread(void *ptr)
{
	struct mpmc_queuebatch *ring = (struct mpmc_queuebatch *)ptr;
	struct mpmc_queuebatch_writer w[1];
	entry_t *entry;
	uint64_t seq = 0;
	struct timeval t[2];
	gettimeofday(&t[0], NULL);

	mpmc_queuebatch_writer_init(w, ring);

	for (;;) {
		if ((entry = (entry_t *)mpmc_queuebatch_writer_prepare(w)) ==
		    NULL) {
			usleep(1);
			continue;
		}

		memset(entry, 0, sizeof(*entry));
		entry->a = seq;
		entry->b = seq + 1;
		seq++;

		mpmc_queuebatch_writer_commit(w, (void *)entry);
		if (seq >= MAXSEQ)
			break;
	}

	gettimeofday(&t[1], NULL);
	printf("write time = %lf\n",
	       (double)t[1].tv_sec * 1000000 + t[1].tv_usec -
		       (double)t[0].tv_sec * 1000000 - t[0].tv_usec);
	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t pub[N], sub[M];
	double u = 0;

	int i, j = 0;
	entry_t *entry;
	struct timeval t[2];
	struct mpmc_queuebatch *r =
		mpmc_queuebatch_create(655360, sizeof(*entry));
	struct mpmc_queuebatch_writer w[1];

	if (argc > 1)
		MAXSEQ = atoll(argv[1]);

	// read_write();

	gettimeofday(&t[0], NULL);
	for (i = 0; i < M; i++)
		pthread_create(&sub[i], NULL, read_thread, r);

	for (i = 0; i < N; i++)
		pthread_create(&pub[i], NULL, write_thread, r);

	for (i = 0; i < N; i++)
		pthread_join(pub[i], NULL);

	mpmc_queuebatch_writer_init(w, r);
	for (j = 0; j < Q; j++) {
		while ((entry = (entry_t *)mpmc_queuebatch_writer_prepare(w)) ==
		       NULL) {
		}
		entry->a = UINT64_MAX;
		entry->b = 0;
		mpmc_queuebatch_writer_commit(w, entry);
	}

	for (i = 0; i < M; i++)
		pthread_join(sub[i], NULL);
	gettimeofday(&t[1], NULL);

	u = (double)t[1].tv_sec + (double)t[1].tv_usec / 1000000 -
	    (double)t[0].tv_sec - (double)t[0].tv_usec / 1000000 -
	    200000.0 / 1000000;

	printf("time = %lfs, %lf/s\n", u, MAXSEQ * N / u);

	__sync_synchronize();
	mpmc_queuebatch_destroy(r);

	return 0;
}
