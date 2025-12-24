#include <stdio.h>
#include <string.h>

#include "ringbuffer.h"

int main()
{
	// 创建环形缓冲区：64KB缓冲区，最大消息4KB
	struct ringbuffer *ring = ringbuffer_create(64 * 1024, 4 * 1024);
	if (!ring) {
		perror("Failed to create ringbuffer");
		return 1;
	}

	printf("RingBuffer created: capacity=%zu, max_msg=%zu\n",
	       ringbuffer_capacity(ring), ring->max_msg_size);

	// 生产者：准备并提交消息
	const char *message = "Hello, RingBuffer!";
	size_t msg_len = strlen(message) + 1;

	void *write_ptr = ringbuffer_writer_prepare(ring, msg_len);
	if (write_ptr) {
		memcpy(write_ptr, message, msg_len);
		ringbuffer_writer_commit(ring, write_ptr);
		printf("Message written: %s\n", message);
	}

	// 消费者：读取消息
	struct reader_result result;
	if (ringbuffer_reader_prepare(ring, &result)) {
		size_t msg_size;
		void *read_ptr;

		while ((read_ptr = ringbuffer_result_next(&result,
							  &msg_size)) != NULL) {
			printf("Message read: %s (size=%zu)\n",
			       (char *)read_ptr, msg_size);
		}

		ringbuffer_reader_commit(ring, &result);
	}

	ringbuffer_destroy(ring);
	return 0;
}
