#include <stdatomic.h>
#include <stdint.h>

atomic_uint_fast64_t counter = ATOMIC_VAR_INIT(0);

void atomic_add(uint64_t value)
{
	atomic_fetch_add_explicit(&counter, value, memory_order_relaxed);
}

uint64_t get_value()
{
	return atomic_load_explicit(&counter, memory_order_relaxed);
}

void increment()
{
	atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed);
}
