#include "mpmc_queue.h"

#include <errno.h>
#include <string.h>

// 平台特定的头文件
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#else
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

// 原子操作包装
#define atomic_compare_exchange_weak(ptr, expected, desired)   \
	__atomic_compare_exchange_n(ptr, expected, desired, 0, \
				    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)

#define atomic_load(ptr) __atomic_load_n(ptr, __ATOMIC_ACQUIRE)
#define atomic_store(ptr, val) __atomic_store_n(ptr, val, __ATOMIC_RELEASE)
#define atomic_fetch_add(ptr, val) \
	__atomic_fetch_add(ptr, val, __ATOMIC_ACQ_REL)

// 内存屏障
#define compiler_barrier() __atomic_signal_fence(__ATOMIC_SEQ_CST)
#define read_barrier() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define write_barrier() __atomic_thread_fence(__ATOMIC_RELEASE)
#define memory_barrier() __atomic_thread_fence(__ATOMIC_SEQ_CST)

// 常量定义
#define TAIL_IDX 0xFFFFFFFF
#define UNUSED_FLAG 0xFFFFFFFE
#define INVALID_IDX 0xFFFFFFFD

// 对齐的类型定义
typedef uint32_t ALIGNED(CACHE_LINE_SIZE) aligned_uint32_t;
typedef uint64_t ALIGNED(CACHE_LINE_SIZE) aligned_uint64_t;

// 容器宏
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))

struct item {
	volatile uint32_t next;
	char content[];
};

struct mpmc_queue {
	size_t nmemb; // 队列容量
	size_t size; // 每个元素的大小（包括item头）
	size_t item_size; // 用户数据大小

	ALIGNED(CACHE_LINE_SIZE) volatile aligned_uint32_t head;
	ALIGNED(CACHE_LINE_SIZE) volatile aligned_uint64_t free;

	char items[]; // 柔性数组，存储所有元素
};

struct mpmc_queuebatch {
	ALIGNED(CACHE_LINE_SIZE) volatile aligned_uint32_t wseq;
	int qnum;
	struct mpmc_queue *queues[];
};

struct mpmc_queuebatch_reader {
	struct mpmc_queuebatch *queuebatch;
	uint32_t queueid;
};

struct mpmc_queuebatch_writer {
	struct mpmc_queuebatch *queuebatch;
	struct mpmc_queue *queue;
};

struct reader_result {
	struct mpmc_queue *q;
	struct item *header;
	struct item *tail;
	struct item *curr;
	size_t nmemb;
};

// 辅助宏
#define ITEM(q, i) ((struct item *)((q)->items + (i) * (q)->size))
#define ITEM_IDX(q, ptr) (((char *)(ptr) - (q)->items) / (q)->size)
#define CONTENT_PTR(item) ((item)->content)
#define ITEM_FROM_CONTENT(ptr) container_of(ptr, struct item, content)

// 跨平台CPU数量获取
int get_cpunum(void)
{
	static int num_cpus = 0;

	if (num_cpus > 0)
		return num_cpus;

#ifdef _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	num_cpus = sysinfo.dwNumberOfProcessors;
#elif defined(__APPLE__)
	int nm[2] = { CTL_HW, HW_NCPU };
	size_t len = sizeof(num_cpus);
	if (sysctl(nm, 2, &num_cpus, &len, NULL, 0) == -1) {
		num_cpus = 1;
	}
#else
	num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (num_cpus <= 0) {
		num_cpus = 1;
	}
#endif

	return num_cpus > 0 ? num_cpus : 1;
}

// 创建队列
struct mpmc_queue *mpmc_queue_create(size_t nmemb, size_t size)
{
	struct mpmc_queue *q = NULL;
	size_t total_size, item_size;
	uint32_t i;

	// 参数检查
	if (nmemb == 0 || size == 0) {
		errno = EINVAL;
		return NULL;
	}

	// 计算对齐后的item大小
	item_size = ((size + sizeof(struct item) + 7) / 8) * 8;
	total_size = sizeof(*q) + item_size * nmemb;

	// 检查溢出
	if (total_size < sizeof(*q)) {
		errno = EOVERFLOW;
		return NULL;
	}

	// 分配内存
	q = (struct mpmc_queue *)calloc(1, total_size);
	if (UNLIKELY(q == NULL)) {
		errno = ENOMEM;
		return NULL;
	}

	q->nmemb = nmemb;
	q->size = item_size;
	q->item_size = size;
	atomic_store(&q->head, TAIL_IDX);
	atomic_store(&q->free, 0); // free高32位是索引，低32位是版本号

	// 初始化空闲链表
	for (i = 0; i < nmemb - 1; i++) {
		ITEM(q, i)->next = i + 1;
	}
	ITEM(q, i)->next = TAIL_IDX;

	// 设置free指针指向第一个空闲节点
	uint64_t initial_free = ((uint64_t)0 << 32) | 1;
	atomic_store(&q->free, initial_free);

	memory_barrier();
	return q;
}

void mpmc_queue_destroy(struct mpmc_queue *q)
{
	if (q) {
		free(q);
	}
}

// 批量队列创建
struct mpmc_queuebatch *mpmc_queuebatch_create(size_t nmemb, size_t size)
{
	struct mpmc_queuebatch *qs = NULL;
	int qnum, i;

	if (nmemb == 0 || size == 0) {
		errno = EINVAL;
		return NULL;
	}

	qnum = get_cpunum();
	QUEUE_ASSERT(qnum > 0, "Invalid CPU number");

	// 分配批量队列结构
	qs = (struct mpmc_queuebatch *)calloc(
		1, sizeof(*qs) + sizeof(struct mpmc_queue *) * qnum);
	if (UNLIKELY(qs == NULL)) {
		errno = ENOMEM;
		return NULL;
	}

	// 计算每个队列的大小
	size_t queue_nmemb = (nmemb + qnum - 1) / qnum;
	if (queue_nmemb < 2)
		queue_nmemb = 2; // 至少2个元素

	qs->qnum = qnum;
	atomic_store(&qs->wseq, 0);

	// 创建子队列
	for (i = 0; i < qnum; i++) {
		qs->queues[i] = mpmc_queue_create(queue_nmemb, size);
		if (UNLIKELY(qs->queues[i] == NULL)) {
			goto error;
		}
	}

	return qs;

error:
	if (qs) {
		for (int j = 0; j < i; j++) {
			if (qs->queues[j]) {
				mpmc_queue_destroy(qs->queues[j]);
			}
		}
		free(qs);
	}
	return NULL;
}

void mpmc_queuebatch_destroy(struct mpmc_queuebatch *qs)
{
	if (qs) {
		for (int i = 0; i < qs->qnum; i++) {
			if (qs->queues[i]) {
				mpmc_queue_destroy(qs->queues[i]);
			}
		}
		free(qs);
	}
}

void mpmc_queuebatch_reader_init(struct mpmc_queuebatch_reader *reader,
				 struct mpmc_queuebatch *qs)
{
	QUEUE_ASSERT(reader != NULL, "Reader is NULL");
	QUEUE_ASSERT(qs != NULL, "Queue batch is NULL");

	reader->queueid = 0;
	reader->queuebatch = qs;
}

void mpmc_queuebatch_writer_init(struct mpmc_queuebatch_writer *writer,
				 struct mpmc_queuebatch *qs)
{
	QUEUE_ASSERT(writer != NULL, "Writer is NULL");
	QUEUE_ASSERT(qs != NULL, "Queue batch is NULL");

	uint32_t seq = atomic_fetch_add(&qs->wseq, 1);
	writer->queue = qs->queues[seq % qs->qnum];
	writer->queuebatch = qs;
}

// 生产者准备数据
void *mpmc_queue_writer_prepare(struct mpmc_queue *q)
{
	QUEUE_ASSERT(q != NULL, "Queue is NULL");

	uint64_t oldf, newf;
	uint32_t freen, seq;
	struct item *ret = NULL;

	for (;;) {
		read_barrier();

		oldf = atomic_load(&q->free);
		freen = (uint32_t)(oldf >> 32); // 空闲节点索引
		seq = (uint32_t)(oldf & 0xFFFFFFFF); // 版本号

		// 检查队列是否已满
		if (UNLIKELY(freen == TAIL_IDX)) {
			return NULL;
		}

		// 检查索引有效性
		if (UNLIKELY(freen >= q->nmemb)) {
			// 数据损坏，尝试恢复或返回错误
			return NULL;
		}

		ret = ITEM(q, freen);
		newf = ((uint64_t)ret->next << 32) | (seq + 1);

		// 使用CAS获取空闲节点
		if (atomic_compare_exchange_weak(&q->free, &oldf, newf)) {
			// 标记节点为已使用
			ret->next = UNUSED_FLAG;
			write_barrier();
			return CONTENT_PTR(ret);
		}

		cpu_relax();
	}
}

void *mpmc_queuebatch_writer_prepare(struct mpmc_queuebatch_writer *writer)
{
	QUEUE_ASSERT(writer != NULL, "Writer is NULL");
	QUEUE_ASSERT(writer->queue != NULL, "Writer queue is NULL");

	return mpmc_queue_writer_prepare(writer->queue);
}

// 生产者提交数据
void mpmc_queue_writer_commit(struct mpmc_queue *q, void *ptr)
{
	QUEUE_ASSERT(q != NULL, "Queue is NULL");
	QUEUE_ASSERT(ptr != NULL, "Pointer is NULL");

	struct item *item = ITEM_FROM_CONTENT(ptr);
	uint32_t idx = ITEM_IDX(q, item);
	uint32_t expected_head, desired_head;

	QUEUE_ASSERT(idx < q->nmemb, "Item index out of bounds");
	QUEUE_ASSERT(item->next == UNUSED_FLAG, "Item not prepared properly");

	// 设置节点next为当前head，然后将head指向当前节点
	for (;;) {
		read_barrier();
		expected_head = atomic_load(&q->head);

		// 将新节点插入链表头部
		item->next = expected_head;
		write_barrier();

		desired_head = idx;

		if (atomic_compare_exchange_weak(&q->head, &expected_head,
						 desired_head)) {
			write_barrier();
			return;
		}

		cpu_relax();
	}
}

void mpmc_queuebatch_writer_commit(struct mpmc_queuebatch_writer *writer,
				   void *ptr)
{
	QUEUE_ASSERT(writer != NULL, "Writer is NULL");
	QUEUE_ASSERT(writer->queue != NULL, "Writer queue is NULL");

	mpmc_queue_writer_commit(writer->queue, ptr);
}

// 消费者遍历数据
void *mpmc_queue_reader_next(struct reader_result *res)
{
	QUEUE_ASSERT(res != NULL, "Result is NULL");

	if (res->curr == NULL) {
		res->curr = res->header;
		return res->curr ? CONTENT_PTR(res->curr) : NULL;
	}

	if (res->curr->next == TAIL_IDX) {
		QUEUE_ASSERT(res->curr == res->tail, "Tail pointer mismatch");
		return NULL;
	}

	QUEUE_ASSERT(res->curr->next < res->q->nmemb, "Invalid next pointer");
	res->curr = ITEM(res->q, res->curr->next);

	return CONTENT_PTR(res->curr);
}

void *mpmc_queuebatch_reader_next(struct reader_result *res)
{
	return mpmc_queue_reader_next(res);
}

// 消费者准备读取批次
size_t mpmc_queue_reader_prepare(struct mpmc_queue *q,
				 struct reader_result *ret)
{
	QUEUE_ASSERT(q != NULL, "Queue is NULL");
	QUEUE_ASSERT(ret != NULL, "Result is NULL");

	uint32_t head, prev, next;
	struct item *item, *tail;
	size_t n = 0;

	// 获取整个链表
	for (;;) {
		read_barrier();
		head = atomic_load(&q->head);

		if (head == TAIL_IDX) {
			return 0; // 队列为空
		}

		// 尝试将head设置为TAIL_IDX，表示我们正在处理这个链表
		uint32_t expected = head;
		if (atomic_compare_exchange_weak(&q->head, &expected,
						 TAIL_IDX)) {
			break;
		}

		cpu_relax();
	}

	// 反转链表以便顺序消费
	QUEUE_ASSERT(head < q->nmemb, "Invalid head index");

	tail = item = ITEM(q, head);
	prev = TAIL_IDX;
	n = 1;

	while (item->next != TAIL_IDX) {
		next = item->next;
		QUEUE_ASSERT(next < q->nmemb, "Invalid next index");

		item->next = prev;
		prev = head;
		head = next;
		item = ITEM(q, head);
		n++;
	}

	// 最后一个节点指向反转后的前一个节点
	item->next = prev;

	ret->nmemb = n;
	ret->q = q;
	ret->header = item; // 反转后的头节点
	ret->tail = tail; // 原始的头节点，现在的尾节点
	ret->curr = NULL;

	return n;
}

size_t mpmc_queuebatch_reader_prepare(struct mpmc_queuebatch_reader *reader,
				      struct reader_result *ret)
{
	QUEUE_ASSERT(reader != NULL, "Reader is NULL");
	QUEUE_ASSERT(reader->queuebatch != NULL, "Queue batch is NULL");
	QUEUE_ASSERT(ret != NULL, "Result is NULL");

	int qnum = reader->queuebatch->qnum;

	for (int i = 0; i < qnum; i++) {
		struct mpmc_queue *q =
			reader->queuebatch->queues[(reader->queueid + i) % qnum];
		size_t n = mpmc_queue_reader_prepare(q, ret);
		if (n > 0) {
			reader->queueid = (reader->queueid + i + 1) % qnum;
			return n;
		}
	}

	reader->queueid = (reader->queueid + qnum) % qnum;
	return 0;
}

// 消费者提交已处理的数据
void mpmc_queue_reader_commit(struct mpmc_queue *q, struct reader_result *res)
{
	QUEUE_ASSERT(q != NULL, "Queue is NULL");
	QUEUE_ASSERT(res != NULL, "Result is NULL");
	QUEUE_ASSERT(res->q == q, "Queue mismatch in reader commit");

	uint64_t oldf, newf;
	uint32_t freen, seq;
	uint32_t idx = ITEM_IDX(q, res->header);

	QUEUE_ASSERT(idx < q->nmemb, "Invalid header index");
	QUEUE_ASSERT(res->tail != NULL, "Tail is NULL");

	for (;;) {
		read_barrier();
		oldf = atomic_load(&q->free);
		freen = (uint32_t)(oldf >> 32);
		seq = (uint32_t)(oldf & 0xFFFFFFFF);

		// 将整个批次链接到空闲链表
		res->tail->next = freen;
		write_barrier();

		newf = ((uint64_t)idx << 32) | (seq + 1);

		if (atomic_compare_exchange_weak(&q->free, &oldf, newf)) {
			write_barrier();
			return;
		}

		cpu_relax();
	}
}

void mpmc_queuebatch_reader_commit(struct mpmc_queuebatch_reader *reader,
				   struct reader_result *res)
{
	QUEUE_ASSERT(reader != NULL, "Reader is NULL");
	QUEUE_ASSERT(res != NULL, "Result is NULL");
	QUEUE_ASSERT(res->q != NULL, "Result queue is NULL");

	mpmc_queue_reader_commit(res->q, res);
}

// 工具函数
bool mpmc_queue_empty(struct mpmc_queue *q)
{
	if (UNLIKELY(q == NULL))
		return true;
	return atomic_load(&q->head) == TAIL_IDX;
}

bool mpmc_queue_full(struct mpmc_queue *q)
{
	if (UNLIKELY(q == NULL))
		return true;

	uint64_t free_val = atomic_load(&q->free);
	uint32_t freen = (uint32_t)(free_val >> 32);
	return freen == TAIL_IDX;
}

size_t mpmc_queue_capacity(struct mpmc_queue *q)
{
	return q ? q->nmemb : 0;
}

size_t mpmc_queue_available(struct mpmc_queue *q)
{
	if (UNLIKELY(q == NULL))
		return 0;

	// 这是一个近似值，在并发环境下可能不精确
	uint32_t head = atomic_load(&q->head);
	if (head == TAIL_IDX)
		return q->nmemb;

	// 计算已使用的大致数量
	uint64_t free_val = atomic_load(&q->free);
	uint32_t freen = (uint32_t)(free_val >> 32);

	// 这只是一个估计值
	size_t used = 0;
	uint32_t current = head;
	while (current != TAIL_IDX && used < q->nmemb) {
		used++;
		struct item *item = ITEM(q, current);
		current = item->next;
	}

	return q->nmemb - used;
}
