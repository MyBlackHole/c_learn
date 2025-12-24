/**
 * 改进版本 - 修复ABA问题、内存序问题、可移植性问题
 */
#ifndef MPMC_QUEUE_H
#define MPMC_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// 编译器检测
#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(e) __builtin_expect(!!(e), 1)
#define UNLIKELY(e) __builtin_expect(!!(e), 0)
#define FORCE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define LIKELY(e) (e)
#define UNLIKELY(e) (e)
#define FORCE_INLINE __forceinline
#else
#define LIKELY(e) (e)
#define UNLIKELY(e) (e)
#define FORCE_INLINE inline
#endif

// 缓存行大小
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

// 内存对齐宏
#if defined(__GNUC__) || defined(__clang__)
#define ALIGNED(n) __attribute__((aligned(n)))
#elif defined(_MSC_VER)
#define ALIGNED(n) __declspec(align(n))
#else
#define ALIGNED(n)
#endif

// 平台无关的CPU放松
#if defined(__x86_64__) || defined(__i386__)
#define cpu_relax() __asm__ volatile("pause\n" ::: "memory")
#elif defined(__aarch64__) || defined(__arm__)
#define cpu_relax() asm volatile("yield\n" ::: "memory")
#else
#define cpu_relax() ((void)0) // 编译屏障
#endif

// 跨平台CPU数量获取
int get_cpunum(void);

// 调试断言
#ifdef NDEBUG
#define QUEUE_ASSERT(cond, msg) ((void)0)
#else
#include <stdio.h>
#include <stdlib.h>
#define QUEUE_ASSERT(cond, msg)                                               \
	do {                                                                  \
		if (!(cond)) {                                                \
			fprintf(stderr,                                       \
				"Queue assertion failed: %s at %s:%d\n", msg, \
				__FILE__, __LINE__);                          \
			abort();                                              \
		}                                                             \
	} while (0)
#endif

// 数据结构前向声明
struct mpmc_queue;
struct mpmc_queuebatch;
struct mpmc_queuebatch_reader;
struct mpmc_queuebatch_writer;
struct reader_result;

// 基本队列操作
struct mpmc_queue *mpmc_queue_create(size_t nmemb, size_t size);
void mpmc_queue_destroy(struct mpmc_queue *q);
void *mpmc_queue_writer_prepare(struct mpmc_queue *q);
void mpmc_queue_writer_commit(struct mpmc_queue *q, void *ptr);
size_t mpmc_queue_reader_prepare(struct mpmc_queue *q,
				 struct reader_result *ret);
void *mpmc_queue_reader_next(struct reader_result *res);
void mpmc_queue_reader_commit(struct mpmc_queue *q, struct reader_result *res);

// 批量队列操作
struct mpmc_queuebatch *mpmc_queuebatch_create(size_t nmemb, size_t size);
void mpmc_queuebatch_destroy(struct mpmc_queuebatch *qs);
void mpmc_queuebatch_reader_init(struct mpmc_queuebatch_reader *reader,
				 struct mpmc_queuebatch *qs);
void mpmc_queuebatch_writer_init(struct mpmc_queuebatch_writer *writer,
				 struct mpmc_queuebatch *qs);
void *mpmc_queuebatch_writer_prepare(struct mpmc_queuebatch_writer *writer);
void mpmc_queuebatch_writer_commit(struct mpmc_queuebatch_writer *writer,
				   void *ptr);
size_t mpmc_queuebatch_reader_prepare(struct mpmc_queuebatch_reader *reader,
				      struct reader_result *ret);
void *mpmc_queuebatch_reader_next(struct reader_result *res);
void mpmc_queuebatch_reader_commit(struct mpmc_queuebatch_reader *reader,
				   struct reader_result *res);

// 工具函数
bool mpmc_queue_empty(struct mpmc_queue *q);
bool mpmc_queue_full(struct mpmc_queue *q);
size_t mpmc_queue_capacity(struct mpmc_queue *q);
size_t mpmc_queue_available(struct mpmc_queue *q);

#ifdef __cplusplus
}
#endif

#endif // MPMC_QUEUE_H
