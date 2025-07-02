target("sys_socket_learn_timeout_client")
    set_kind("binary")
    add_files("client.c")


target("sys_socket_learn_timeout_server")
    set_kind("binary")
    add_files("server.c")
