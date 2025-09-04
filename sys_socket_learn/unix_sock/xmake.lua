target("sys_socket_learn_unix_sock_client")
    set_kind("binary")
    add_files("client.c")


target("sys_socket_learn_unix_sock_server")
    set_kind("binary")
    add_files("server.c")
