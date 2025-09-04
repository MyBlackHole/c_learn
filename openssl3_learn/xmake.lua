add_requires("openssl3", {system = false, configs = {static = true}, debug = is_mode("debug")})

target("openssl_learn_SHA1_test")
    set_kind("binary")
    add_files("SHA1_test.c")
    add_packages("openssl3")


target("openssl_learn_RSA_test")
    set_kind("binary")
    add_files("RSA_test.c")
    add_packages("openssl3")

target("openssl_learn_AES_test")
    set_kind("binary")
    add_files("AES_test.c")
    add_packages("openssl3")

target("openssl_learn_DES_test")
    set_kind("binary")
    add_files("DES_test.c")
    add_packages("openssl3")

target("openssl_learn_MD5_test")
    set_kind("binary")
    add_files("MD5_test.c")
    add_packages("openssl3")


