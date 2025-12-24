add_requires("openssl")

target("demo_time_compressed")
    set_kind("binary")
    add_files("main.c")

target("demo_time_compressed1")
    set_kind("binary")
    add_files("main1.c")
    add_packages("openssl")
