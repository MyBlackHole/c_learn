#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	int sockfd;
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(argv[1]);
	addr.sin_port = htons(atoi(argv[2]));

	printf("Connecting to %s:%s...\n", argv[1], argv[2]);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return EXIT_FAILURE;
	}
	// int keepalive = 1; // 开启TCP KeepAlive功能
	// int keepidle = 1; // tcp_keepalive_time
	// int keepcnt = 1; // tcp_keepalive_probes
	// int keepintvl = 1; // tcp_keepalive_intvl
	//
	// if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive,
	// 	       sizeof(keepalive)) ||
	//     setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&keepidle,
	// 	       sizeof(keepidle)) ||
	//     setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, (void *)&keepcnt,
	// 	       sizeof(keepcnt)) ||
	//     setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&keepintvl,
	// 	       sizeof(keepintvl))) {
	// 	perror("setsockopt");
	// 	return EXIT_FAILURE;
	// }

	int keepintvl = 10; // tcp_user_timeout
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_USER_TIMEOUT, (void *)&keepintvl, sizeof(keepintvl)))
	{
		perror("setsockopt");
		return EXIT_FAILURE;
	}

	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("connect");
		return EXIT_FAILURE;
	}
	printf("Connected to %s:%s\n", argv[1], argv[2]);
	while (1) {
		sleep(1);
	}

	return EXIT_SUCCESS;
}


//  加 keepalive 选项，连接超时。
// ❯ time /run/media/black/Data/Documents/c/build/linux/x86_64/debug/sys_socket_learn_tcp_keepalive_client 10.0.1.1 6666
// Connecting to 10.0.1.1:6666...
// connect: Connection timed out
//  10.0.1.1 6666  0.00s user 0.00s system 0% cpu 2:14.66 total
//
//  没有加 keepalive 选项，连接超时。
// ❯ time /run/media/black/Data/Documents/c/build/linux/x86_64/debug/sys_socket_learn_tcp_keepalive_client 10.0.1.1 6666
// Connecting to 10.0.1.1:6666...
// connect: Connection timed out
//  10.0.1.1 6666  0.00s user 0.00s system 0% cpu 2:13.94 total
//
//  加 TCP_USER_TIMEOUT 选项，连接超时。
// ❯ time /run/media/black/Data/Documents/c/build/linux/x86_64/debug/sys_socket_learn_tcp_keepalive_client 10.0.1.1 6666
// Connecting to 10.0.1.1:6666...
// connect: Connection timed out
//  10.0.1.1 6666  0.00s user 0.00s system 0% cpu 1.059 total
