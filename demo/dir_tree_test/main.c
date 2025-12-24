#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#define SEPARATOR "/"

// 函数声明
void enumerate_files(int dir_fd, const char *path, int depth);
void print_file_info(const char *path, const char *filename,
		     struct stat *file_stat);
char *get_permissions(mode_t mode);
char *get_file_type(mode_t mode);

int main(int argc, char *argv[])
{
	char *root_path;
	int root_fd;

	if (argc < 2) {
		printf("使用方法: %s <目录路径>\n", argv[0]);
		printf("示例: %s .\n", argv[0]);
		printf("示例: %s /home/user\n", argv[0]);
		root_path = ".";
	} else {
		root_path = argv[1];
	}

	// 打开根目录的文件描述符
	root_fd = open(root_path, O_RDONLY | O_DIRECTORY);
	if (root_fd == -1) {
		perror("打开根目录失败");
		return 1;
	}

	enumerate_files(root_fd, root_path, 0);

	close(root_fd);
	return 0;
}

// 递归枚举文件和目录
void enumerate_files(int base_fd, const char *path, int depth)
{
	int dir_fd;
	DIR *dir;
	char *path_tmp = strdup(path);
	struct dirent *entry;
	struct stat file_stat;
	char full_path[1024];

	// 使用 openat 打开目录，然后使用 fdopendir
	dir_fd = openat(base_fd, ".", O_RDONLY | O_DIRECTORY);
	if (dir_fd == -1) {
		perror("openat失败");
		free(path_tmp);
		return;
	}

	if (!(dir = fdopendir(dir_fd))) {
		perror("fdopendir失败");
		close(dir_fd);
		free(path_tmp);
		return;
	}

	if (path_tmp[strlen(path) - 1] == SEPARATOR[0]) {
		path_tmp[strlen(path) - 1] = '\0';
	}

	while ((entry = readdir(dir)) != NULL) {
		// 跳过当前目录和父目录
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		// 构建完整路径
		snprintf(full_path, sizeof(full_path), "%s%s%s", path_tmp,
			 SEPARATOR, entry->d_name);

		// 使用 fstatat 获取文件信息
		if (fstatat(dir_fd, entry->d_name, &file_stat,
			    AT_SYMLINK_NOFOLLOW) == -1) {
			perror("fstatat失败");
			continue;
		}

		// 打印文件信息
		print_file_info(path_tmp, entry->d_name, &file_stat);

		// 如果是目录，递归枚举（排除符号链接）
		if (S_ISDIR(file_stat.st_mode) && !S_ISLNK(file_stat.st_mode)) {
			int subdir_fd = openat(dir_fd, entry->d_name,
					       O_RDONLY | O_DIRECTORY);
			if (subdir_fd != -1) {
				enumerate_files(subdir_fd, full_path,
						depth + 1);
				close(subdir_fd);
			} else {
				perror("打开子目录失败");
			}
		}
	}

	closedir(dir); // 这会自动关闭 dir_fd
	free(path_tmp);
}

// 打印文件信息（保持不变）
void print_file_info(const char *path, const char *filename,
		     struct stat *file_stat)
{
	char time_buf[80];
	struct passwd *pwd;
	struct group *grp;

	// 获取用户名和组名
	pwd = getpwuid(file_stat->st_uid);
	grp = getgrgid(file_stat->st_gid);

	// 格式化时间
	strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S",
		 localtime(&file_stat->st_mtime));

	printf("%s%s%s\n", path, SEPARATOR, filename);
	printf("  类型: %s\n", get_file_type(file_stat->st_mode));
	printf("  权限: %s\n", get_permissions(file_stat->st_mode));
	printf("  大小: %lld 字节\n", (long long)file_stat->st_size);
	printf("  所有者: %s (%d)\n", pwd ? pwd->pw_name : "未知",
	       file_stat->st_uid);
	printf("  组: %s (%d)\n", grp ? grp->gr_name : "未知",
	       file_stat->st_gid);
	printf("  修改时间: %s\n", time_buf);
	printf("  节点号:%llu\n", (unsigned long long)file_stat->st_ino);
	printf("\n");
}

// 获取权限字符串（保持不变）
char *get_permissions(mode_t mode)
{
	static char perm[10];

	perm[0] = (mode & S_IRUSR) ? 'r' : '-';
	perm[1] = (mode & S_IWUSR) ? 'w' : '-';
	perm[2] = (mode & S_IXUSR) ? 'x' : '-';
	perm[3] = (mode & S_IRGRP) ? 'r' : '-';
	perm[4] = (mode & S_IWGRP) ? 'w' : '-';
	perm[5] = (mode & S_IXGRP) ? 'x' : '-';
	perm[6] = (mode & S_IROTH) ? 'r' : '-';
	perm[7] = (mode & S_IWOTH) ? 'w' : '-';
	perm[8] = (mode & S_IXOTH) ? 'x' : '-';
	perm[9] = '\0';

	return perm;
}

// 获取文件类型（保持不变）
char *get_file_type(mode_t mode)
{
	if (S_ISREG(mode))
		return "普通文件";
	if (S_ISDIR(mode))
		return "目录";
	if (S_ISCHR(mode))
		return "字符设备";
	if (S_ISBLK(mode))
		return "块设备";
	if (S_ISFIFO(mode))
		return "FIFO/管道";
	if (S_ISLNK(mode))
		return "符号链接";
	if (S_ISSOCK(mode))
		return "套接字";
	return "未知";
}
