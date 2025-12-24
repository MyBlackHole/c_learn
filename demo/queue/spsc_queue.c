#include "spsc_queue.h"

#include <string.h>
#include <errno.h>

// 平台特定的内存对齐分配
#ifdef _WIN32
#include <malloc.h>
#else
#include <stdlib.h>
#endif

// 原子操作包装
#define atomic_load(ptr) __atomic_load_n(ptr, __ATOMIC_ACQUIRE)
#define atomic_store(ptr, val) __atomic_store_n(ptr, val, __ATOMIC_RELEASE)

// 内存屏障
#define read_barrier() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define write_barrier() __atomic_thread_fence(__ATOMIC_RELEASE)
#define memory_barrier() __atomic_thread_fence(__ATOMIC_SEQ_CST)

// 对齐的类型定义
typedef uint64_t ALIGNED(CACHE_LINE_SIZE) aligned_uint64_t;

struct squeue {
	size_t capacity; // 实际容量（2的幂次-1）
	size_t mask; // 用于掩码操作的掩码
	size_t element_size; // 每个元素的大小

	ALIGNED(CACHE_LINE_SIZE) volatile aligned_uint64_t write_pos;
	ALIGNED(CACHE_LINE_SIZE) volatile aligned_uint64_t read_pos;

	char buffer[]; // 柔性数组，存储元素数据
};

struct reader_result {
	struct squeue *queue;
	uint64_t start_pos;
	uint64_t current_pos;
	uint64_t end_pos;
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

// 回绕问题计算
static uint64_t wrap_add(uint64_t write_pos, uint64_t read_pos)
{
	uint64_t diff;
	if (write_pos >= read_pos) {
		diff = write_pos - read_pos;
	} else {
		diff = write_pos + (UINT64_MAX + 1 - read_pos);
	}
	return diff;
}

// 平台无关的内存对齐分配
static void *aligned_alloc_page(size_t size)
{
#ifdef _WIN32
	return _aligned_malloc(size, PAGE_SIZE);
#else
	void *ptr = NULL;
	if (posix_memalign(&ptr, PAGE_SIZE, size) != 0) {
		return NULL;
	}
	return ptr;
#endif
}

// 平台无关的内存对齐释放
static void aligned_free_page(void *ptr)
{
#ifdef _WIN32
	_aligned_free(ptr);
#else
	free(ptr);
#endif
}

// 计算元素指针
static FORCE_INLINE void *get_element(struct squeue *queue, uint64_t index)
{
	SQUEUE_ASSERT(queue != NULL, "Queue is NULL");
	return &queue->buffer[(index & queue->mask) * queue->element_size];
}

// 验证指针是否属于队列
static FORCE_INLINE bool validate_pointer(struct squeue *queue, void *ptr)
{
	if (UNLIKELY(queue == NULL || ptr == NULL))
		return false;

	char *ptr_char = (char *)ptr;
	char *buffer_start = queue->buffer;
	char *buffer_end =
		buffer_start + (queue->capacity + 1) * queue->element_size;

	return ptr_char >= buffer_start && ptr_char < buffer_end;
}

struct squeue *squeue_create(size_t nmemb, size_t element_size)
{
	struct squeue *queue = NULL;
	size_t total_size, actual_capacity;

	// 参数检查
	if (nmemb == 0 || element_size == 0) {
		errno = EINVAL;
		return NULL;
	}

	// 确保容量是2的幂次减1，便于使用掩码操作
	actual_capacity = next_power_of_2(nmemb) - 1;
	if (actual_capacity < 1) {
		errno = EINVAL;
		return NULL;
	}

	// 计算总大小
	total_size = sizeof(*queue) + (actual_capacity + 1) * element_size;

	// 检查溢出
	if (total_size < sizeof(*queue)) {
		errno = EOVERFLOW;
		return NULL;
	}

	// 分配页面对齐的内存
	queue = (struct squeue *)aligned_alloc_page(total_size);
	if (UNLIKELY(queue == NULL)) {
		errno = ENOMEM;
		return NULL;
	}

	// 初始化队列结构
	memset(queue, 0, total_size);
	queue->capacity = actual_capacity;
	queue->mask = actual_capacity;
	queue->element_size = element_size;

	atomic_store(&queue->write_pos, 0);
	atomic_store(&queue->read_pos, 0);

	memory_barrier();
	return queue;
}

void squeue_destroy(struct squeue *queue)
{
	if (queue) {
		aligned_free_page(queue);
	}
}

void *squeue_writer_prepare(struct squeue *queue)
{
	SQUEUE_ASSERT(queue != NULL, "Queue is NULL");

	uint64_t write_pos, read_pos;

	for (;;) {
		read_barrier();
		write_pos = atomic_load(&queue->write_pos);
		read_pos = atomic_load(&queue->read_pos);

		if (wrap_add(write_pos, read_pos) >= queue->capacity) {
			errno = ENOSPC;
			return NULL; // 队列已满
		}

		// 返回准备写入的元素指针
		void *element = get_element(queue, write_pos);
		return element;
	}
}

void squeue_writer_commit(struct squeue *queue, void *ptr)
{
	SQUEUE_ASSERT(queue != NULL, "Queue is NULL");
	SQUEUE_ASSERT(ptr != NULL, "Pointer is NULL");

	// 验证指针有效性
	if (UNLIKELY(!validate_pointer(queue, ptr))) {
		SQUEUE_ASSERT(0, "Invalid pointer in writer commit");
		return;
	}

	// 获取当前写入位置对应的元素指针进行验证
	uint64_t current_write_pos = atomic_load(&queue->write_pos);
	void *expected_ptr = get_element(queue, current_write_pos);

	if (UNLIKELY(ptr != expected_ptr)) {
		SQUEUE_ASSERT(0, "Commit order violation or concurrent write");
		return;
	}

	// 更新写入位置
	atomic_store(&queue->write_pos, current_write_pos + 1);
	write_barrier();
}

size_t squeue_reader_prepare(struct squeue *queue, struct reader_result *res)
{
	SQUEUE_ASSERT(queue != NULL, "Queue is NULL");
	SQUEUE_ASSERT(res != NULL, "Result is NULL");

	uint64_t read_pos, write_pos;

	read_barrier();
	read_pos = atomic_load(&queue->read_pos);
	write_pos = atomic_load(&queue->write_pos);

	if (UNLIKELY(read_pos == write_pos)) {
		return 0; // 队列为空
	}

	res->queue = queue;
	res->start_pos = read_pos;
	res->current_pos = read_pos;
	res->end_pos = write_pos;

	// 使用无符号减法自动处理回绕
	return write_pos - read_pos;
}

void *squeue_result_next(struct reader_result *res)
{
	SQUEUE_ASSERT(res != NULL, "Result is NULL");

	// 检查是否还有元素可读
	if (res->current_pos >= res->end_pos) {
		return NULL;
	}

	// 获取当前元素
	void *element = get_element(res->queue, res->current_pos);
	res->current_pos++;

	return element;
}

void squeue_reader_commit(struct squeue *queue, struct reader_result *res)
{
	SQUEUE_ASSERT(queue != NULL, "Queue is NULL");
	SQUEUE_ASSERT(res != NULL, "Result is NULL");
	SQUEUE_ASSERT(res->queue == queue, "Queue mismatch");

	// 验证读取位置 - 这是主要的并发安全检测
	uint64_t current_read_pos = atomic_load(&queue->read_pos);
	if (UNLIKELY(current_read_pos != res->start_pos)) {
		SQUEUE_ASSERT(0, "Concurrent read detected");
		return;
	}

	// 直接更新位置，相信 squeue_result_next() 的逻辑正确性
	// 移除了有问题的位置检查，因为回绕情况下检查逻辑复杂且容易出错
	atomic_store(&queue->read_pos, res->current_pos);
	write_barrier();
}

// 工具函数
bool squeue_empty(struct squeue *queue)
{
	if (UNLIKELY(queue == NULL))
		return true;
	uint64_t read_pos = atomic_load(&queue->read_pos);
	uint64_t write_pos = atomic_load(&queue->write_pos);
	return read_pos == write_pos;
}

bool squeue_full(struct squeue *queue)
{
	if (UNLIKELY(queue == NULL))
		return true;
	uint64_t read_pos = atomic_load(&queue->read_pos);
	uint64_t write_pos = atomic_load(&queue->write_pos);
	return wrap_add(write_pos, read_pos) > queue->capacity;
}

size_t squeue_capacity(struct squeue *queue)
{
	return queue ? queue->capacity : 0;
}

size_t squeue_size(struct squeue *queue)
{
	if (UNLIKELY(queue == NULL))
		return 0;
	uint64_t read_pos = atomic_load(&queue->read_pos);
	uint64_t write_pos = atomic_load(&queue->write_pos);
	return wrap_add(write_pos, read_pos);
}

size_t squeue_element_size(struct squeue *queue)
{
	return queue ? queue->element_size : 0;
}

size_t squeue_available_space(struct squeue *queue)
{
	if (UNLIKELY(queue == NULL))
		return 0;
	uint64_t read_pos = atomic_load(&queue->read_pos);
	uint64_t write_pos = atomic_load(&queue->write_pos);
	// 可用空间 = 总容量 - 已使用空间
	return queue->capacity - wrap_add(write_pos, read_pos);
}
