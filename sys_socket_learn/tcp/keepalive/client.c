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

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return EXIT_FAILURE;
	}
	int keepalive = 1; // 开启TCP KeepAlive功能
	int keepidle = 10; // tcp_keepalive_time
	int keepcnt = 9; // tcp_keepalive_probes
	int keepintvl = 6; // tcp_keepalive_intvl

	if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive,
		       sizeof(keepalive)) ||
	    setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (void *)&keepidle,
		       sizeof(keepidle)) ||
	    setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (void *)&keepcnt,
		       sizeof(keepcnt)) ||
	    setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (void *)&keepintvl,
		       sizeof(keepintvl))) {
		perror("setsockopt");
		return EXIT_FAILURE;
	}

	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("connect");
		return EXIT_FAILURE;
	}
	while (1) {
		sleep(1);
	}

	return EXIT_SUCCESS;
}
