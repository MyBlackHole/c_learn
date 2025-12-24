/**
 * 
 * 优化版本 - 修复内存序问题、边界检查、可移植性问题、回绕问题
 */
#ifndef SQUEUE_H
#define SQUEUE_H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 编译器优化
#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(e) __builtin_expect(!!(e), 1)
#define UNLIKELY(e) __builtin_expect(!!(e), 0)
#define FORCE_INLINE inline __attribute__((always_inline))
#else
#define LIKELY(e) (e)
#define UNLIKELY(e) (e)
#define FORCE_INLINE inline
#endif

// 缓存行大小
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

// 页面大小
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

// 内存对齐
#if defined(__GNUC__) || defined(__clang__)
#define ALIGNED(n) __attribute__((aligned(n)))
#elif defined(_MSC_VER)
#define ALIGNED(n) __declspec(align(n))
#else
#define ALIGNED(n)
#endif

// 调试断言
#ifdef NDEBUG
#define SQUEUE_ASSERT(cond, msg) ((void)0)
#else
#include <stdio.h>
#include <stdlib.h>
#define SQUEUE_ASSERT(cond, msg)                                               \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr,                                        \
				"SQueue assertion failed: %s at %s:%d\n", msg, \
				__FILE__, __LINE__);                           \
			abort();                                               \
		}                                                              \
	} while (0)
#endif

// 前向声明
struct squeue;
struct reader_result;

// 队列操作
struct squeue *squeue_create(size_t nmemb, size_t size);
void squeue_destroy(struct squeue *queue);

void *squeue_writer_prepare(struct squeue *queue);
void squeue_writer_commit(struct squeue *queue, void *ptr);

size_t squeue_reader_prepare(struct squeue *queue, struct reader_result *res);
void *squeue_result_next(struct reader_result *res);
void squeue_reader_commit(struct squeue *queue, struct reader_result *res);

// 工具函数
bool squeue_empty(struct squeue *queue);
bool squeue_full(struct squeue *queue);
size_t squeue_capacity(struct squeue *queue);
size_t squeue_size(struct squeue *queue);
size_t squeue_element_size(struct squeue *queue);
size_t squeue_available_space(struct squeue *queue);

#ifdef __cplusplus
}
#endif

#endif // SQUEUE_H
