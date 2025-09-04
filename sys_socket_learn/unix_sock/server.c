#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define UNIXSTR_PATH "/tmp/unix_sock.sock"

int main(int argc, char *argv[])
{
	int listenfd;
	struct sockaddr_un servaddr, cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	int ret;

	listenfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (listenfd < 0) {
		printf("socket failed.\n");
		return EXIT_FAILURE;
	}

	unlink(UNIXSTR_PATH);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, UNIXSTR_PATH);

	ret = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if (ret < 0) {
		printf("bind failed. errno = %d.\n", errno);
		close(listenfd);
		return EXIT_FAILURE;
	}

	listen(listenfd, 5);

	printf("server started.\n");
	while (1) {
		char buf[1024];
		int n;
		int connfd;
		connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
		if (connfd < 0) {
			printf("accept failed. errno = %d.\n", errno);
			continue;
		}
		printf("new connection from %s.\n", cliaddr.sun_path);

		while (1) {
			n = read(connfd, buf, sizeof(buf));
			if (n <= 0) {
				break;
			}
			printf("recv: %s\n", buf);
			write(connfd, buf, n);
		}
		close(connfd);
	}
	close(listenfd);

	return EXIT_SUCCESS;
}
