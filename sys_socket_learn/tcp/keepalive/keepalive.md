❯ xmake run sys_socket_learn_tcp_keepalive_client 127.0.0.1 6666
^C

❯ xmake run sys_socket_learn_tcp_keepalive_server
======waiting for client's request======
======new client connected======
j
======client disconnected======

❯ sudo tcpdump -i lo port 6666
tcpdump: verbose output suppressed, use -v[v]... for full protocol decode
listening on lo, link-type EN10MB (Ethernet), snapshot length 262144 bytes
22:15:48.590793 IP localhost.60988 > localhost.6666: Flags [S], seq 3965502491, win 65495, options [mss 65495,sackOK,TS val 3241278595 ecr 0,nop,wscale 7], length 0
22:15:48.590819 IP localhost.6666 > localhost.60988: Flags [S.], seq 990124298, ack 3965502492, win 65483, options [mss 65495,sackOK,TS val 3241278595 ecr 3241278595,nop,wscale 7], length 0
22:15:48.590846 IP localhost.60988 > localhost.6666: Flags [.], ack 1, win 512, options [nop,nop,TS val 3241278595 ecr 3241278595], length 0

# 这几个就是心跳包
22:15:58.845547 IP localhost.60988 > localhost.6666: Flags [.], ack 1, win 512, options [nop,nop,TS val 3241288850 ecr 3241278595], length 0
22:15:58.845564 IP localhost.6666 > localhost.60988: Flags [.], ack 1, win 512, options [nop,nop,TS val 3241288850 ecr 3241278595], length 0
22:16:08.893537 IP localhost.60988 > localhost.6666: Flags [.], ack 1, win 512, options [nop,nop,TS val 3241298898 ecr 3241288850], length 0
22:16:08.893554 IP localhost.6666 > localhost.60988: Flags [.], ack 1, win 512, options [nop,nop,TS val 3241298898 ecr 3241278595], length 0
22:16:18.941523 IP localhost.60988 > localhost.6666: Flags [.], ack 1, win 512, options [nop,nop,TS val 3241308946 ecr 3241298898], length 0
22:16:18.941541 IP localhost.6666 > localhost.60988: Flags [.], ack 1, win 512, options [nop,nop,TS val 3241308946 ecr 3241278595], length 0

22:16:25.815847 IP localhost.6666 > localhost.60988: Flags [F.], seq 1, ack 1, win 512, options [nop,nop,TS val 3241315820 ecr 3241278595], length 0
22:16:25.817524 IP localhost.60988 > localhost.6666: Flags [.], ack 2, win 512, options [nop,nop,TS val 3241315822 ecr 3241315820], length 0
22:16:29.252878 IP localhost.60988 > localhost.6666: Flags [F.], seq 1, ack 2, win 512, options [nop,nop,TS val 3241319257 ecr 3241315820], length 0
22:16:29.252904 IP localhost.6666 > localhost.60988: Flags [.], ack 2, win 512, options [nop,nop,TS val 3241319257 ecr 3241319257], length 0
