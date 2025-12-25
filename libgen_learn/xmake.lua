local dir_path = path.relative(os.curdir(), os.projectdir())

target("libgen_learn_basename_test")
    set_kind("binary")
    add_files("basename_test.c")

target("libgen_learn_dirname")
    set_kind("binary")
    add_files("dirname.c")


