#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

/* 超时连接 */
int timeout_connect(const char *ip, int port, int time)
{
	int ret = 0;
	struct sockaddr_in servaddr;

	printf("client start...\n");

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &servaddr.sin_addr);
	servaddr.sin_port = htons(port);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	assert(sockfd >= 0);

	/* 通过选项SO_RCVTIMEO和SO_SNDTIMEO设置的超时时间的类型时timeval, 和select系统调用的超时参数类型相同 */
	struct timeval timeout;
	timeout.tv_sec = time;
	timeout.tv_usec = 0;

	socklen_t len = sizeof(timeout);
	ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
	if (ret == -1) {
		perror("setsockopt error");

		return -1;
	}

	if ((ret = connect(sockfd, (struct sockaddr *)&servaddr,
			   sizeof(servaddr))) < 0) {
		/* 超时对于errno 为EINPROGRESS. 下面条件如果成立，就可以处理定时任务了 */
		if (errno == EINPROGRESS) {
			perror("connecting timeout, process timeout logic");
			return -1;
		}

		perror("error occur when connecting to server\n");
	}

	return sockfd;
}

int main(int argc, char *argv[])
{
	if (argc <= 3) {
		printf("usage: %s ip_address port_number timeout\n", argv[0]);
		return 1;
	}

	const char *ip = argv[1];
	int port = atoi(argv[2]);
	int time = atoi(argv[3]);

	printf("connect %s:%d:%d...\n", ip, port, time);

	int sockfd = timeout_connect(ip, port, time);
	if (sockfd < 0) {
		perror("timeout_connect error");
		return 1;
	}

	return 0;
}


// ❯ xmake run sys_socket_learn_timeout_connect 192.168.10.10 6611 10
// connect 192.168.10.10:6611:10...
// client start...
// connecting timeout, process timeout logic: Operation now in progress
// timeout_connect error: Operation now in progress
