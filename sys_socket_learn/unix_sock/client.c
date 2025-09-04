#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define UNIXSTR_PATH "/tmp/unix_sock.sock"

int main(int argc, char *argv[])
{
	int clifd;
	struct sockaddr_un servaddr;
	int ret;

	clifd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (clifd < 0) {
		printf("socket failed.\n");
		return EXIT_FAILURE;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, UNIXSTR_PATH);

	ret = connect(clifd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if (ret < 0) {
		printf("connect failed.\n");
		return EXIT_SUCCESS;
	}

	while (1) {
		char buf[1024];
		int n;
		bzero(buf, sizeof(buf));
		printf("input message:");
		fgets(buf, sizeof(buf), stdin);
		n = write(clifd, buf, strlen(buf));
		if (n < 0) {
			printf("write failed.\n");
			return EXIT_FAILURE;
		}
		bzero(buf, sizeof(buf));
		n = read(clifd, buf, sizeof(buf));
		if (n < 0) {
			printf("read failed.\n");
			return EXIT_FAILURE;
		}
		printf("server response: %s", buf);
	}
	close(clifd);
	return EXIT_SUCCESS;
}
