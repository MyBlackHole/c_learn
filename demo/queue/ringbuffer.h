/**
 * 优化版本 - 修复内存序问题、边界检查、可移植性问题
 */
#ifndef RINGBUFFER_H
#define RINGBUFFER_H

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

// 内存屏障
#if defined(__GNUC__) || defined(__clang__)
#define COMPILER_BARRIER() asm volatile("" ::: "memory")
#else
#define COMPILER_BARRIER()
#endif

// 缓存行大小
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
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
#define RINGBUF_ASSERT(cond, msg) ((void)0)
#else
#include <stdio.h>
#include <stdlib.h>
#define RINGBUF_ASSERT(cond, msg)                                             \
	do {                                                                  \
		if (!(cond)) {                                                \
			fprintf(stderr,                                       \
				"RingBuffer assertion failed: %s at %s:%d\n", \
				msg, __FILE__, __LINE__);                     \
			abort();                                              \
		}                                                             \
	} while (0)
#endif

// 前向声明
struct ringbuffer;
struct reader_result;

// 环形缓冲区操作
struct ringbuffer *ringbuffer_create(size_t size, size_t max_msg_size);
void ringbuffer_destroy(struct ringbuffer *ring);

void *ringbuffer_writer_prepare(struct ringbuffer *ring, size_t size);
void ringbuffer_writer_commit(struct ringbuffer *ring, void *ptr);

int ringbuffer_reader_prepare(struct ringbuffer *ring,
			      struct reader_result *res);
void *ringbuffer_result_next(struct reader_result *res, size_t *size);
void ringbuffer_reader_commit(struct ringbuffer *ring,
			      struct reader_result *res);

// 工具函数
bool ringbuffer_empty(struct ringbuffer *ring);
bool ringbuffer_full(struct ringbuffer *ring);
size_t ringbuffer_capacity(struct ringbuffer *ring);
size_t ringbuffer_available_space(struct ringbuffer *ring);
size_t ringbuffer_used_space(struct ringbuffer *ring);

#ifdef __cplusplus
}
#endif

#endif // RINGBUFFER_H
