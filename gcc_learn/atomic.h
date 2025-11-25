#include <stdint.h>
#include <stdio.h>

uint64_t counter = 0;

void atomic_add(uint64_t value)
{
	__sync_fetch_and_add(&counter, value);
}

uint64_t get_value()
{
	// 使用原子加载
	return __atomic_load_n(&counter, __ATOMIC_RELAXED);
}

// 更现代的内置函数
void modern_atomic_add(uint64_t value)
{
	__atomic_fetch_add(&counter, value, __ATOMIC_RELAXED);
}
