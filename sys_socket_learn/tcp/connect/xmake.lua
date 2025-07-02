target("sys_socket_learn_tcp_connect_client")
    set_kind("binary")
    add_files("client.c")


target("sys_socket_learn_tcp_connect_server")
    set_kind("binary")
    add_files("server.c")
