target("mpmc_queue")
    set_kind("static")
    add_files("mpmc_queue.c")

target("ringbuffer")
    set_kind("static")
    add_files("ringbuffer.c")

target("spsc_queue")
    set_kind("static")
    add_files("spsc_queue.c")

target("queue_mpmc_test")
    set_kind("binary")
    add_files("mpmc_test.c")
    add_deps("mpmc_queue")

target("queue_ring_test")
    set_kind("binary")
    add_files("ring_test.c")
    add_deps("ringbuffer")

target("queue_spsc_test")
    set_kind("binary")
    add_files("spsc_test.c")
    add_deps("spsc_queue")
