target("ld_preload_hook")
    set_kind("shared")
    add_files("hook.c")

target("ld_preload_def")
    set_kind("shared")
    add_files("def.c")

target("ld_preload_main")
    set_kind("binary")
    add_files("main.c")
    add_deps("ld_preload_def")
