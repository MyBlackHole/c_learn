#include "ringbuffer.h"

#include <string.h>
#include <errno.h>

// 原子操作包装
#define atomic_load(ptr) __atomic_load_n(ptr, __ATOMIC_ACQUIRE)
#define atomic_store(ptr, val) __atomic_store_n(ptr, val, __ATOMIC_RELEASE)

// 内存屏障
#define read_barrier() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define write_barrier() __atomic_thread_fence(__ATOMIC_RELEASE)
#define memory_barrier() __atomic_thread_fence(__ATOMIC_SEQ_CST)

// 容器宏
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))

// 对齐的类型定义
typedef uint64_t ALIGNED(CACHE_LINE_SIZE) aligned_uint64_t;

struct item {
	uint32_t size;
	char contents[];
};

struct ringbuffer {
	size_t size; // 缓冲区总大小（必须是2的幂）
	size_t max_msg_size; // 最大消息大小
	size_t mask; // 用于模运算的掩码

	ALIGNED(CACHE_LINE_SIZE) volatile aligned_uint64_t write_pos;
	ALIGNED(CACHE_LINE_SIZE) volatile aligned_uint64_t read_pos;

	char buffer[]; // 柔性数组，实际缓冲区
};

struct reader_result {
	struct ringbuffer *ring;
	uint64_t start_pos;
	uint64_t end_pos;
	uint64_t current_pos;
};

// 工具函数：计算大于等于输入的最小2的幂
static FORCE_INLINE uint32_t next_power_of_2(uint32_t n)
{
	if (n == 0)
		return 1;
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n + 1;
}

// 工具函数：对齐到缓存行
static FORCE_INLINE size_t align_to_cache_line(size_t size)
{
	return (size + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
}

// 计算消息总大小（包括头部）
static FORCE_INLINE size_t total_message_size(size_t data_size)
{
	return align_to_cache_line(sizeof(struct item) + data_size);
}

// 位置计算宏
#define POSITION_MASK(ring) ((ring)->mask)
#define WRAP_POSITION(ring, pos) ((pos) & POSITION_MASK(ring))
#define ITEM_AT_POS(ring, pos) \
	((struct item *)((ring)->buffer + WRAP_POSITION(ring, pos)))

struct ringbuffer *ringbuffer_create(size_t size, size_t max_msg_size)
{
	struct ringbuffer *ring = NULL;
	size_t total_size, buffer_size;

	// 参数检查
	if (size == 0 || max_msg_size == 0) {
		errno = EINVAL;
		return NULL;
	}

	// 确保缓冲区大小是2的幂
	size = next_power_of_2(size);
	if (size < max_msg_size + sizeof(struct item)) {
		errno = EINVAL; // 缓冲区太小
		return NULL;
	}

	// 计算总分配大小
	buffer_size = align_to_cache_line(size);
	total_size = sizeof(*ring) + buffer_size;

	// 检查溢出
	if (total_size < sizeof(*ring)) {
		errno = EOVERFLOW;
		return NULL;
	}

	// 分配内存
	ring = (struct ringbuffer *)calloc(1, total_size);
	if (UNLIKELY(ring == NULL)) {
		errno = ENOMEM;
		return NULL;
	}

	ring->size = size;
	ring->max_msg_size = max_msg_size;
	ring->mask = size - 1;

	atomic_store(&ring->write_pos, 0);
	atomic_store(&ring->read_pos, 0);

	memory_barrier();
	return ring;
}

void ringbuffer_destroy(struct ringbuffer *ring)
{
	if (ring) {
		free(ring);
	}
}

void *ringbuffer_writer_prepare(struct ringbuffer *ring, size_t size)
{
	RINGBUF_ASSERT(ring != NULL, "RingBuffer is NULL");

	if (UNLIKELY(size == 0 || size > ring->max_msg_size)) {
		errno = EINVAL;
		return NULL;
	}

	uint64_t write_pos, read_pos;
	size_t required_size = total_message_size(size);
	size_t available_space;

	// 检查可用空间
	for (;;) {
		read_barrier();
		write_pos = atomic_load(&ring->write_pos);
		read_pos = atomic_load(&ring->read_pos);

		// 计算可用空间
		if (write_pos >= read_pos) {
			available_space = ring->size - (write_pos - read_pos);
		} else {
			available_space = read_pos - write_pos;
		}

		// 预留一些空间避免边界情况
		if (available_space <= required_size + CACHE_LINE_SIZE) {
			errno = ENOSPC;
			return NULL;
		}

		// 检查是否跨越缓冲区边界
		size_t space_until_wrap = ring->size - (write_pos & ring->mask);
		if (UNLIKELY(space_until_wrap < sizeof(struct item))) {
			// 需要包装到缓冲区开始处
			struct item *padding = ITEM_AT_POS(ring, write_pos);
			padding->size = 0; // 特殊标记：填充项

			write_pos = (write_pos + space_until_wrap) &
				    ~ring->mask;
			atomic_store(&ring->write_pos, write_pos);
			continue; // 重新检查空间
		}

		// 检查当前写入位置是否有足够连续空间
		if (space_until_wrap < required_size) {
			// 需要包装，先插入填充项
			struct item *padding = ITEM_AT_POS(ring, write_pos);
			padding->size = space_until_wrap; // 填充剩余空间

			write_pos = (write_pos + space_until_wrap) &
				    ~ring->mask;
			atomic_store(&ring->write_pos, write_pos);
			continue; // 重新检查空间
		}

		// 准备消息项
		struct item *item = ITEM_AT_POS(ring, write_pos);
		item->size = size;

		return item->contents;
	}
}

void ringbuffer_writer_commit(struct ringbuffer *ring, void *ptr)
{
	RINGBUF_ASSERT(ring != NULL, "RingBuffer is NULL");
	RINGBUF_ASSERT(ptr != NULL, "Pointer is NULL");

	struct item *item = container_of(ptr, struct item, contents);
	uint64_t current_pos = atomic_load(&ring->write_pos);
	size_t item_size = total_message_size(item->size);

	// 验证提交位置
	uint64_t expected_pos = WRAP_POSITION(ring, current_pos);
	struct item *expected_item = ITEM_AT_POS(ring, current_pos);
	RINGBUF_ASSERT(item == expected_item, "Invalid commit order");

	// 更新写入位置
	uint64_t new_pos = current_pos + item_size;
	atomic_store(&ring->write_pos, new_pos);
	write_barrier();
}

int ringbuffer_reader_prepare(struct ringbuffer *ring,
			      struct reader_result *res)
{
	RINGBUF_ASSERT(ring != NULL, "RingBuffer is NULL");
	RINGBUF_ASSERT(res != NULL, "Result is NULL");

	uint64_t read_pos, write_pos;

	read_barrier();
	read_pos = atomic_load(&ring->read_pos);
	write_pos = atomic_load(&ring->write_pos);

	if (UNLIKELY(read_pos == write_pos)) {
		return 0; // 缓冲区为空
	}

	res->ring = ring;
	res->start_pos = read_pos;
	res->end_pos = write_pos;
	res->current_pos = read_pos;

	return 1;
}

void *ringbuffer_result_next(struct reader_result *res, size_t *size)
{
	RINGBUF_ASSERT(res != NULL, "Result is NULL");

	struct ringbuffer *ring = res->ring;

	// 检查是否还有数据
	if (res->current_pos >= res->end_pos) {
		return NULL;
	}

	// 获取当前消息
	struct item *item = ITEM_AT_POS(ring, res->current_pos);

	// 处理填充项
	if (item->size == 0) {
		// 跳过填充项
		size_t space_until_wrap =
			ring->size - (res->current_pos & ring->mask);
		res->current_pos = (res->current_pos + space_until_wrap) &
				   ~ring->mask;

		if (res->current_pos >= res->end_pos) {
			return NULL;
		}

		item = ITEM_AT_POS(ring, res->current_pos);
	}

	// 验证消息大小
	if (UNLIKELY(item->size > ring->max_msg_size)) {
		RINGBUF_ASSERT(0, "Corrupted message size");
		return NULL;
	}

	// 返回消息内容和大小
	if (size != NULL) {
		*size = item->size;
	}

	// 更新当前位置
	size_t item_total_size = total_message_size(item->size);
	res->current_pos += item_total_size;

	// 处理包装
	if ((res->current_pos & ring->mask) + sizeof(struct item) >
	    ring->size) {
		res->current_pos = (res->current_pos + ring->size - 1) &
				   ~ring->mask;
	}

	return item->contents;
}

void ringbuffer_reader_commit(struct ringbuffer *ring,
			      struct reader_result *res)
{
	RINGBUF_ASSERT(ring != NULL, "RingBuffer is NULL");
	RINGBUF_ASSERT(res != NULL, "Result is NULL");
	RINGBUF_ASSERT(res->ring == ring, "RingBuffer mismatch");

	// 验证提交位置
	uint64_t current_read_pos = atomic_load(&ring->read_pos);
	RINGBUF_ASSERT(current_read_pos == res->start_pos,
		       "Concurrent read detected");

	// 更新读取位置
	atomic_store(&ring->read_pos, res->current_pos);
	write_barrier();
}

// 工具函数
bool ringbuffer_empty(struct ringbuffer *ring)
{
	if (UNLIKELY(ring == NULL))
		return true;
	uint64_t read_pos = atomic_load(&ring->read_pos);
	uint64_t write_pos = atomic_load(&ring->write_pos);
	return read_pos == write_pos;
}

bool ringbuffer_full(struct ringbuffer *ring)
{
	if (UNLIKELY(ring == NULL))
		return true;

	uint64_t write_pos = atomic_load(&ring->write_pos);
	uint64_t read_pos = atomic_load(&ring->read_pos);
	size_t used_space = (write_pos - read_pos);

	// 预留一些空间避免边界情况
	return used_space >=
	       (ring->size - ring->max_msg_size - sizeof(struct item));
}

size_t ringbuffer_capacity(struct ringbuffer *ring)
{
	return ring ? ring->size : 0;
}

size_t ringbuffer_available_space(struct ringbuffer *ring)
{
	if (UNLIKELY(ring == NULL))
		return 0;

	uint64_t write_pos = atomic_load(&ring->write_pos);
	uint64_t read_pos = atomic_load(&ring->read_pos);

	if (write_pos >= read_pos) {
		return ring->size - (write_pos - read_pos);
	} else {
		return read_pos - write_pos;
	}
}

size_t ringbuffer_used_space(struct ringbuffer *ring)
{
	if (UNLIKELY(ring == NULL))
		return 0;

	uint64_t write_pos = atomic_load(&ring->write_pos);
	uint64_t read_pos = atomic_load(&ring->read_pos);

	return write_pos - read_pos;
}
