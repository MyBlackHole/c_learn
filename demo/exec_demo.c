#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void check_child_process() {
  pid_t child_pid;

  // 非阻塞方式检查子进程状态
  child_pid = waitpid(-1, NULL, WNOHANG);

  if (child_pid > 0) {
    printf("✓ 子进程 %d 已结束并被回收\n", child_pid);
  } else if (child_pid == 0) {
    printf("○ 子进程仍在运行，没有子进程退出\n");
  } else if (child_pid == -1) {
    if (errno == ECHILD) {
      printf("× 没有子进程需要等待\n");
    } else {
      perror("waitpid失败");
    }
  }
}

pid_t execute() {
  pid_t pid = fork();
  if (pid > 0) {
    return pid;
  } else if (pid < 0) {
    printf("fork failed. errno: %d\n", errno);
    return -1;
  } else {
    printf("child process started\n");
    exit(EXIT_SUCCESS);
  }
}

int main(int argc, char *argv[]) {
  pid_t pid = execute();
  if (pid < 0) {
    printf("execute failed. pid: %d\n", pid);
    return EXIT_FAILURE;
  }
  printf("child pid: %d\n", pid);
  sleep(10);

  if (kill(pid, SIGKILL) < 0) {
    perror("kill failed\n");
  } else {
    printf("✓ 已杀死子进程 %d\n", pid);
  }

  check_child_process();

  return EXIT_SUCCESS;
}
