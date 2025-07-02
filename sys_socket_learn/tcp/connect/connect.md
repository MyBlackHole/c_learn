❯ xmake run sys_socket_learn_tcp_connect_server
======waiting for client's request======
======new client connected======
j
======client disconnected======

❯ xmake run sys_socket_learn_tcp_connect_client 127.0.0.1 6666

❯ sudo tcpdump -i lo port 6666
tcpdump: verbose output suppressed, use -v[v]... for full protocol decode
listening on lo, link-type EN10MB (Ethernet), snapshot length 262144 bytes
22:07:15.793794 IP localhost.52230 > localhost.6666: Flags [F.], seq 463655054, ack 212975817, win 512, options [nop,nop,TS val 3240765798 ecr 3240732438], length 0
22:07:15.793824 IP localhost.6666 > localhost.52230: Flags [.], ack 1, win 512, options [nop,nop,TS val 3240765798 ecr 3240765798], length 0
22:07:23.976758 IP localhost.52846 > localhost.6666: Flags [S], seq 2265044857, win 65495, options [mss 65495,sackOK,TS val 3240773981 ecr 0,nop,wscale 7], length 0
22:07:23.976783 IP localhost.6666 > localhost.52846: Flags [S.], seq 1069967601, ack 2265044858, win 65483, options [mss 65495,sackOK,TS val 3240773981 ecr 3240773981,nop,wscale 7], length 0
22:07:23.976804 IP localhost.52846 > localhost.6666: Flags [.], ack 1, win 512, options [nop,nop,TS val 3240773981 ecr 3240773981], length 0
22:07:23.976899 IP localhost.6666 > localhost.52846: Flags [F.], seq 1, ack 1, win 512, options [nop,nop,TS val 3240773981 ecr 3240773981], length 0
22:07:23.978498 IP localhost.52846 > localhost.6666: Flags [.], ack 2, win 512, options [nop,nop,TS val 3240773983 ecr 3240773981], length 0

22:09:29.725824 IP localhost.52846 > localhost.6666: Flags [F.], seq 1, ack 2, win 512, options [nop,nop,TS val 3240899730 ecr 3240773981], length 0
22:09:29.725844 IP localhost.6666 > localhost.52846: Flags [R], seq 1069967603, win 0, length 0
