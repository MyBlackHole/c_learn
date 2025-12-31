add_requires("openssl3", {system = false, configs = {static = true}, debug = is_mode("debug")})

target("libmdb_test")
    set_kind("binary")
    add_files("*.c")
    add_packages("openssl3")
