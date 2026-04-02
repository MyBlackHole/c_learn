local dir_path = path.relative(os.curdir(), os.projectdir())

target(dir_path)
    set_kind("binary")
    add_files("stdlib_learn.c", "EXIT_STATUS.c", "malloc_1_test.c", "free_1_test.c")
    add_files("mkstemp_test.c")
    add_files("mkstemp_test1.c")
