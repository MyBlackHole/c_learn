#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

int timeout_recv(int fd, char *buf, int len, int nsec)
{
	struct timeval timeout;
	timeout.tv_sec = nsec;
	timeout.tv_usec = 0;

	printf("timeout_recv called, timeout %d seconds\n", nsec);

	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) <
	    0) {
		perror("setsockopt error");
		exit(1);
	}

	int n = recv(fd, buf, len, 0);

	return n;
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf("usage: %s <ip address> <port>\n", argv[0]);
	}

	char *ip = argv[1];
	uint16_t port = atoi(argv[2]);

	printf("client start..\n");
	printf("connect to %s:%d\n", ip, port);

	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket error");
		exit(1);
	}

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &servaddr.sin_addr);
	servaddr.sin_port = htons(port);

	int connfd;
	if ((connfd = connect(sockfd, (struct sockaddr *)&servaddr,
			      sizeof(servaddr))) < 0) {
		perror("connect error");
		exit(1);
	}

	printf("success to connect server %s:%d\n", ip, port);
	printf("wait for server's response\n");
	char buf[100];
	while (1) {
		int nread;

		nread = timeout_recv(sockfd, buf, sizeof(buf), 10);
		if (nread < 0) {
			if (errno == EAGAIN) {
				printf("timeout_recv error: Resource temporarily unavailable\n");
				continue;
			}
			perror("timeout_recv error");
			exit(1);
		} else if (nread == 0) {
			shutdown(sockfd, SHUT_RDWR);
			break;
		}

		write(STDOUT_FILENO, buf, nread);
	}

	return 0;
}


// ❯ xmake run sys_socket_learn_timeout_client 127.0.0.1 8001
// client start..
// connect to 127.0.0.1:8001
// success to connect server 127.0.0.1:8001
// wait for server's response
// timeout_recv called, timeout 10 seconds
// timeout_recv error: Resource temporarily unavailable
// timeout_recv called, timeout 10 seconds
// timeout_recv error: Resource temporarily unavailable
// timeout_recv called, timeout 10 seconds
// llll
// timeout_recv called, timeout 10 seconds
// timeout_recv error: Resource temporarily unavailable
// timeout_recv called, timeout 10 seconds
// timeout_recv error: Resource temporarily unavailable
// timeout_recv called, timeout 10 seconds
// timeout_recv error: Resource temporarily unavailable
// timeout_recv called, timeout 10 seconds
