#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

// 配置结构体
struct config {
	size_t file_count;
	size_t min_file_size;
	size_t max_file_size;
	size_t min_depth;
	size_t max_depth;
	size_t max_children_per_dir;
	const char *base_dir;
	bool verbose;
};

// 目录名称组件
const char *dir_components[] = {
	"doc",	   "data",   "files",  "temp",	"work", "projects", "backup",
	"archive", "images", "videos", "audio", "docs", "config",   "system",
	"user",	   "home",   "var",    "etc",	"bin",	"lib",	    "src",
	"build",   "dist",   "test",   "log",	"cache"
};
const int dir_components_count =
	sizeof(dir_components) / sizeof(dir_components[0]);

// 文件名称组件
const char *file_components[] = { "report",   "data",	"file",	   "document",
				  "image",    "video",	"audio",   "config",
				  "settings", "backup", "archive", "temp",
				  "test",     "log",	"readme",  "index",
				  "main",     "module", "library", "source",
				  "build",    "dist",	"cache",   "session" };
const int file_components_count =
	sizeof(file_components) / sizeof(file_components[0]);

// 文件扩展名
const char *file_extensions[] = { "txt",  "dat", "log", "cfg", "ini",  "json",
				  "xml",  "csv", "pdf", "doc", "docx", "xls",
				  "xlsx", "jpg", "png", "gif", "mp3",  "mp4",
				  "avi",  "mov", "zip", "tar", "gz",   "bin" };
const int file_extensions_count =
	sizeof(file_extensions) / sizeof(file_extensions[0]);

// 递归创建目录
int create_directories(const char *path)
{
	char tmp[1024];
	char *p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);

	// 移除末尾的斜杠
	if (len > 0 && tmp[len - 1] == PATH_SEPARATOR) {
		tmp[len - 1] = 0;
	}

	// 逐级创建目录
	for (p = tmp + 1; *p; p++) {
		if (*p == PATH_SEPARATOR) {
			*p = 0;

			// 检查目录是否存在，不存在则创建
			if (access(tmp, F_OK) != 0) {
				if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
					return -1;
				}
			}

			*p = PATH_SEPARATOR;
		}
	}

	// 创建最终目录
	if (access(tmp, F_OK) != 0) {
		if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
			return -1;
		}
	}

	return 0;
}

// 生成随机文件名
void generate_random_filename(char *filename, size_t max_length)
{
	const char *prefix = file_components[rand() % file_components_count];
	const char *suffix = file_extensions[rand() % file_extensions_count];

	// 有时添加数字后缀
	if (rand() % 3 == 0) {
		snprintf(filename, max_length, "%s_%d.%s", prefix,
			 rand() % 1000, suffix);
	} else {
		snprintf(filename, max_length, "%s.%s", prefix, suffix);
	}
}

// 生成随机目录名
void generate_random_dirname(char *dirname, size_t max_length)
{
	const char *component = dir_components[rand() % dir_components_count];

	// 有时添加数字后缀
	if (rand() % 3 == 0) {
		snprintf(dirname, max_length, "%s_%d", component, rand() % 100);
	} else {
		snprintf(dirname, max_length, "%s", component);
	}
}

// 创建并填充随机文件
int create_random_file(const char *full_path, size_t min_size, size_t max_size)
{
	size_t file_size = min_size + (rand() % (max_size - min_size + 1));

	// 创建并打开文件
	FILE *file = fopen(full_path, "wb");
	if (!file) {
		fprintf(stderr, "Failed to create file: %s\n", full_path);
		return -1;
	}

	// 生成随机数据并写入文件
	unsigned char *buffer = malloc(file_size);
	if (!buffer) {
		fclose(file);
		fprintf(stderr, "Memory allocation failed\n");
		return -1;
	}

	for (size_t i = 0; i < file_size; i++) {
		buffer[i] = rand() % 256;
	}

	size_t written = fwrite(buffer, 1, file_size, file);
	free(buffer);
	fclose(file);

	if (written != file_size) {
		fprintf(stderr, "Failed to write all data to file: %s\n",
			full_path);
		return -1;
	}

	return 0;
}

// 递归创建目录结构和文件
int create_directory_structure(const char *base_path, size_t current_depth,
			       size_t max_depth, size_t max_children,
			       size_t *files_created, size_t target_files,
			       size_t min_file_size, size_t max_file_size,
			       bool verbose)
{
	if (*files_created >= target_files) {
		return 0;
	}

	// 在当前目录中创建一些文件
	int files_to_create = rand() % (max_children / 2 + 1);
	for (int i = 0; i < files_to_create && *files_created < target_files;
	     i++) {
		char filename[256];
		char full_path[2048];

		generate_random_filename(filename, sizeof(filename));
		snprintf(full_path, sizeof(full_path), "%s%c%s", base_path,
			 PATH_SEPARATOR, filename);

		if (create_random_file(full_path, min_file_size,
				       max_file_size) == 0) {
			(*files_created)++;
			if (verbose) {
				printf("Created file: %s\n", full_path);
			}
		}
	}

	// 如果还没有达到最大深度，创建子目录
	if (current_depth < max_depth && *files_created < target_files) {
		int dirs_to_create = 1 + rand() % (max_children - 1);
		if (dirs_to_create > max_children)
			dirs_to_create = max_children;

		for (int i = 0;
		     i < dirs_to_create && *files_created < target_files; i++) {
			char dirname[256];
			char new_path[2048];

			generate_random_dirname(dirname, sizeof(dirname));
			snprintf(new_path, sizeof(new_path), "%s%c%s",
				 base_path, PATH_SEPARATOR, dirname);

			// 创建子目录
			if (create_directories(new_path) == 0) {
				if (verbose) {
					printf("Created directory: %s\n",
					       new_path);
				}

				// 递归创建子目录结构
				create_directory_structure(
					new_path, current_depth + 1, max_depth,
					max_children, files_created,
					target_files, min_file_size,
					max_file_size, verbose);
			}
		}
	}

	return 0;
}

// 打印使用说明
void print_usage(const char *program_name)
{
	printf("Usage: %s [options]\n", program_name);
	printf("Options:\n");
	printf("  -c COUNT     Number of files to create (default: 100)\n");
	printf("  -min SIZE    Minimum file size in bytes (default: 1024)\n");
	printf("  -max SIZE    Maximum file size in bytes (default: 1048576)\n");
	printf("  -mind DEPTH  Minimum directory depth (default: 2)\n");
	printf("  -maxd DEPTH  Maximum directory depth (default: 5)\n");
	printf("  -child N     Maximum children per directory (default: 8)\n");
	printf("  -d DIR       Base directory (default: ./test_files)\n");
	printf("  -v           Verbose output\n");
	printf("  -h           Show this help message\n");
}

// 解析命令行参数
int parse_arguments(int argc, char *argv[], struct config *cfg)
{
	// 设置默认值
	cfg->file_count = 100;
	cfg->min_file_size = 1024; // 1KB
	cfg->max_file_size = 1048576; // 1MB
	cfg->min_depth = 2;
	cfg->max_depth = 5;
	cfg->max_children_per_dir = 8;
	cfg->base_dir = "./test_files";
	cfg->verbose = false;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
			cfg->file_count = atol(argv[++i]);
		} else if (strcmp(argv[i], "-min") == 0 && i + 1 < argc) {
			cfg->min_file_size = atol(argv[++i]);
		} else if (strcmp(argv[i], "-max") == 0 && i + 1 < argc) {
			cfg->max_file_size = atol(argv[++i]);
		} else if (strcmp(argv[i], "-mind") == 0 && i + 1 < argc) {
			cfg->min_depth = atol(argv[++i]);
		} else if (strcmp(argv[i], "-maxd") == 0 && i + 1 < argc) {
			cfg->max_depth = atol(argv[++i]);
		} else if (strcmp(argv[i], "-child") == 0 && i + 1 < argc) {
			cfg->max_children_per_dir = atol(argv[++i]);
		} else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
			cfg->base_dir = argv[++i];
		} else if (strcmp(argv[i], "-v") == 0) {
			cfg->verbose = true;
		} else if (strcmp(argv[i], "-h") == 0) {
			print_usage(argv[0]);
			exit(EXIT_SUCCESS);
		} else {
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			print_usage(argv[0]);
			return -1;
		}
	}

	// 验证参数
	if (cfg->min_file_size > cfg->max_file_size) {
		fprintf(stderr,
			"Minimum file size cannot be larger than maximum file size\n");
		return -1;
	}

	if (cfg->min_depth > cfg->max_depth) {
		fprintf(stderr,
			"Minimum depth cannot be larger than maximum depth\n");
		return -1;
	}

	if (cfg->min_depth < 1) {
		fprintf(stderr, "Minimum depth must be at least 1\n");
		return -1;
	}

	if (cfg->max_children_per_dir < 2) {
		fprintf(stderr,
			"Maximum children per directory must be at least 2\n");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct config cfg;
	int result = 0;

	// 解析命令行参数
	if (parse_arguments(argc, argv, &cfg) != 0) {
		return EXIT_FAILURE;
	}

	// 初始化随机数生成器
	srand(time(NULL));

	// 创建基础目录
	if (create_directories(cfg.base_dir) != 0) {
		fprintf(stderr, "Failed to create base directory: %s\n",
			cfg.base_dir);
		return EXIT_FAILURE;
	}

	if (cfg.verbose) {
		printf("Creating %zu files in %s\n", cfg.file_count,
		       cfg.base_dir);
		printf("File sizes between %zu and %zu bytes\n",
		       cfg.min_file_size, cfg.max_file_size);
		printf("Directory depth between %zu and %zu\n", cfg.min_depth,
		       cfg.max_depth);
		printf("Max children per directory: %zu\n",
		       cfg.max_children_per_dir);
	}

	// 创建文件结构
	size_t files_created = 0;
	size_t actual_depth =
		cfg.min_depth + (rand() % (cfg.max_depth - cfg.min_depth + 1));

	result = create_directory_structure(cfg.base_dir, 1, actual_depth,
					    cfg.max_children_per_dir,
					    &files_created, cfg.file_count,
					    cfg.min_file_size,
					    cfg.max_file_size, cfg.verbose);

	if (cfg.verbose) {
		printf("File generation completed. Created %zu files.\n",
		       files_created);
		printf("Result: %s\n", result == 0 ? "success" : "with errors");
	}

	return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
