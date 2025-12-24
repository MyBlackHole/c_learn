#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "spsc_queue.h"

// 测试数据结构
struct test_data {
	int id;
	char message[32];
	double value;
};

// 测试回绕情况的辅助函数
void test_wrap_around(struct squeue *queue)
{
	printf("\n=== Testing Wrap Around ===\n");

	// 模拟接近回绕的情况
	// 注意：这只是一个概念演示，实际中很难真正测试64位整数的回绕

	// 填充队列到接近满
	size_t capacity = squeue_capacity(queue);
	for (size_t i = 0; i < capacity; i++) {
		void *ptr = squeue_writer_prepare(queue);
		if (ptr) {
			struct test_data *data = ptr;
			data->id = i;
			snprintf(data->message, sizeof(data->message),
				 "Message %zu", i);
			data->value = i * 1.1;
			squeue_writer_commit(queue, ptr);
		}
	}

	printf("Queue filled: size=%zu, capacity=%zu, full=%s\n",
	       squeue_size(queue), squeue_capacity(queue),
	       squeue_full(queue) ? "true" : "false");

	// 读取一些数据，腾出空间
	struct reader_result result;
	size_t count = squeue_reader_prepare(queue, &result);
	if (count > 0) {
		// 只读取一部分
		size_t read_count = count / 2;
		for (size_t i = 0; i < read_count; i++) {
			struct test_data *data = squeue_result_next(&result);
			if (data) {
				printf("Read: id=%d, message=%s\n", data->id,
				       data->message);
			}
		}
		squeue_reader_commit(queue, &result);
		printf("Read %zu elements, remaining=%zu\n", read_count,
		       squeue_size(queue));
	}

	// 继续写入，测试回绕逻辑
	for (size_t i = 0; i < 5; i++) {
		void *ptr = squeue_writer_prepare(queue);
		if (ptr) {
			struct test_data *data = ptr;
			data->id = capacity + i;
			snprintf(data->message, sizeof(data->message),
				 "Wrap Message %zu", i);
			data->value = (capacity + i) * 1.1;
			squeue_writer_commit(queue, ptr);
			printf("Written wrap message %zu\n", i);
		}
	}
}

int main()
{
	// 创建队列：容量100个元素，每个元素是test_data结构
	struct squeue *queue = squeue_create(100, sizeof(struct test_data));
	if (!queue) {
		perror("Failed to create squeue");
		return 1;
	}

	printf("Queue created: capacity=%zu, element_size=%zu\n",
	       squeue_capacity(queue), squeue_element_size(queue));

	// 基本功能测试
	printf("\n=== Basic Functionality Test ===\n");

	// 写入测试数据
	struct test_data data = { 1, "Hello, SQueue!", 3.14159 };
	void *write_ptr = squeue_writer_prepare(queue);
	if (write_ptr) {
		memcpy(write_ptr, &data, sizeof(data));
		squeue_writer_commit(queue, write_ptr);
		printf("Data written: id=%d, message=%s, value=%f\n", data.id,
		       data.message, data.value);
	}

	// 再写入一个数据
	data.id = 2;
	data.value = 2.71828;
	strcpy(data.message, "Second message");

	write_ptr = squeue_writer_prepare(queue);
	if (write_ptr) {
		memcpy(write_ptr, &data, sizeof(data));
		squeue_writer_commit(queue, write_ptr);
		printf("Second data written\n");
	}

	// 读取数据
	struct reader_result result;
	size_t count = squeue_reader_prepare(queue, &result);
	if (count > 0) {
		printf("Found %zu elements to read\n", count);

		struct test_data *read_data;
		while ((read_data = squeue_result_next(&result)) != NULL) {
			printf("Data read: id=%d, message=%s, value=%f\n",
			       read_data->id, read_data->message,
			       read_data->value);
		}

		squeue_reader_commit(queue, &result);
		printf("Read commit completed\n");
	}

	// 检查队列状态
	printf("\nQueue status: empty=%s, full=%s, size=%zu/%zu, available=%zu\n",
	       squeue_empty(queue) ? "true" : "false",
	       squeue_full(queue) ? "true" : "false", squeue_size(queue),
	       squeue_capacity(queue), squeue_available_space(queue));

	// 测试回绕情况（概念性）
	test_wrap_around(queue);

	squeue_destroy(queue);
	printf("\nQueue destroyed successfully\n");
	return 0;
}
