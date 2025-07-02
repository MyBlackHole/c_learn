#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

int sockfd = -1;

void sig_func(int sig_no)
{
	if (sig_no == SIGINT || sig_no == SIGTERM) {
		if (sockfd >= 0) {
			close(sockfd);
		}
		exit(1);
	}
}

int main()
{
	struct sockaddr_in servaddr, cliaddr;
	int listenfd;

	signal(SIGINT, sig_func);

	printf("server start...\n");

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket error");
		exit(1);
	}
	sockfd = listenfd;

	int on = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) <
	    0) {
		perror("setsocketopt error");
		exit(1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	// servaddr.sin_addr.s_addr = INADDR_ANY;
	inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
	servaddr.sin_port = htons(8001);

	if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) <
	    0) {
		perror("bind error");
		exit(1);
	}

	if (listen(listenfd, 5) < 0) {
		perror("listen error");
		exit(1);
	}

	char buf[1024];
	socklen_t clilen = sizeof(cliaddr);
	int connfd;

	if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen)) <
	    0) {
		perror("accept error");
		exit(1);
	}

	printf("input a line string: \n");
	int nbytes;
	while (fgets(buf, sizeof(buf), stdin)) {
		nbytes = send(connfd, buf, strlen(buf), 0);
		if (nbytes < 0) {
			perror("send error");
			break;
		} else if (nbytes == 0) {
		}
		printf("send: %s\n", buf);
	}

	close(connfd);
	close(listenfd);

	return 0;
}

// ❯ xmake run sys_socket_learn_timeout_server
// server start...
// input a line string:
// llll
// send: llll
