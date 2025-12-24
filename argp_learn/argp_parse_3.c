#include <argp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>

/*** 子命令枚举 ***/
typedef enum {
	CMD_NONE,
	CMD_ADD,
	CMD_REMOVE,
	CMD_LIST,
	CMD_MOVE,
	CMD_SEARCH,
	CMD_STATS
} subcommand_t;

/*** 共享数据结构 ***/
struct arguments {
	subcommand_t cmd; // 当前选择的子命令
	bool verbose; // 全局选项：详细输出
	bool recursive; // 全局选项：递归操作

	// 子命令特定参数
	union {
		struct { // ADD 命令参数
			char *name;
			char *type;
			char *content;
			bool overwrite;
		} add;

		struct { // REMOVE 命令参数
			char *target;
			bool force;
			bool preserve_root;
		} remove;

		struct { // LIST 命令参数
			char *directory;
			bool long_format;
			bool show_hidden;
			bool human_readable;
		} list;

		struct { // MOVE 命令参数
			char *source;
			char *destination;
			bool interactive;
			bool update;
		} move;

		struct { // SEARCH 命令参数
			char *pattern;
			char *location;
			bool case_sensitive;
			bool regex;
		} search;

		struct { // STATS 命令参数
			char *path;
			bool show_permissions;
			bool show_ownership;
			bool show_timestamps;
		} stats;
	} params;
};

/*** 全局选项解析器 ***/
static struct argp_option global_options[] = {
	{ "verbose", 'v', 0, 0, "显示详细输出" },
	{ "recursive", 'r', 0, 0, "递归操作" },
	{ 0 },
};

static error_t parse_global_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key) {
	case 'v':
		arguments->verbose = true;
		break;
	case 'r':
		arguments->recursive = true;
		break;
	case ARGP_KEY_ARG: // 遇到非选项参数（子命令名）
		if (state->arg_num == 0) { // 只处理第一个非选项参数
			if (strcmp(arg, "add") == 0) {
				arguments->cmd = CMD_ADD;
			} else if (strcmp(arg, "remove") == 0) {
				arguments->cmd = CMD_REMOVE;
			} else if (strcmp(arg, "list") == 0) {
				arguments->cmd = CMD_LIST;
			} else if (strcmp(arg, "move") == 0) {
				arguments->cmd = CMD_MOVE;
			} else if (strcmp(arg, "search") == 0) {
				arguments->cmd = CMD_SEARCH;
			} else if (strcmp(arg, "stats") == 0) {
				arguments->cmd = CMD_STATS;
			} else {
				argp_error(
					state,
					"无效子命令: %s\n可用命令: add, remove, list, move, search, stats",
					arg);
			}
		} else {
			// 后续参数传递给子命令解析器
			return ARGP_ERR_UNKNOWN;
		}
		break;
	case ARGP_KEY_END:
		if (arguments->cmd == CMD_NONE) {
			argp_error(
				state,
				"必须指定子命令\n可用命令: add, remove, list, move, search, stats");
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

/*** ADD 子命令解析器 ***/
static struct argp_option add_options[] = {
	{ "name", 'n', "NAME", 0, "文件/目录名称 (必需)" },
	{ "type", 't', "TYPE", 0, "类型: file|dir (默认: file)" },
	{ "content", 'c', "TEXT", 0, "文件内容" },
	{ "overwrite", 'o', 0, 0, "覆盖已存在的文件" },
	{ 0 },
};

static error_t parse_add_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key) {
	case 'n':
		arguments->params.add.name = arg;
		break;
	case 't':
		if (strcmp(arg, "file") != 0 && strcmp(arg, "dir") != 0) {
			argp_error(state, "无效类型: %s (必须是 file 或 dir)",
				   arg);
		}
		arguments->params.add.type = arg;
		break;
	case 'c':
		arguments->params.add.content = arg;
		break;
	case 'o':
		arguments->params.add.overwrite = true;
		break;
	case ARGP_KEY_ARG:
		if (!arguments->params.add.name) {
			arguments->params.add.name = arg;
		} else if (!arguments->params.add.content) {
			arguments->params.add.content = arg;
		} else {
			argp_error(state, "多余参数: %s", arg);
		}
		break;
	case ARGP_KEY_END:
		if (!arguments->params.add.name) {
			argp_error(state, "必须提供名称 (--name)");
		}
		if (!arguments->params.add.type) {
			arguments->params.add.type = "file"; // 默认类型
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp add_argp = {
	add_options,
	parse_add_opt,
	"[名称] [内容]",
	"添加新文件或目录",
};

/*** REMOVE 子命令解析器 ***/
static struct argp_option remove_options[] = {
	{ "target", 't', "PATH", 0, "要删除的目标路径 (必需)" },
	{ "force", 'f', 0, 0, "强制删除，不提示确认" },
	{ "preserve-root", 'p', 0, 0, "防止删除根目录" },
	{ 0 },
};

static error_t parse_remove_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key) {
	case 't':
		arguments->params.remove.target = arg;
		break;
	case 'f':
		arguments->params.remove.force = true;
		break;
	case 'p':
		arguments->params.remove.preserve_root = true;
		break;
	case ARGP_KEY_ARG:
		if (!arguments->params.remove.target) {
			arguments->params.remove.target = arg;
		} else {
			argp_error(state, "多余参数: %s", arg);
		}
		break;
	case ARGP_KEY_END:
		if (!arguments->params.remove.target) {
			argp_error(state, "必须提供目标路径 (--target)");
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp remove_argp = {
	remove_options,
	parse_remove_opt,
	"[目标路径]",
	"删除文件或目录",
};

/*** LIST 子命令解析器 ***/
static struct argp_option list_options[] = {
	{ "directory", 'd', "PATH", 0, "要列出的目录 (默认: 当前目录)" },
	{ "long", 'l', 0, 0, "使用长格式显示详细信息" },
	{ "all", 'a', 0, 0, "显示隐藏文件" },
	{ "human", 'h', 0, 0, "以人类可读格式显示文件大小" },
	{ 0 },
};

static error_t parse_list_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key) {
	case 'd':
		arguments->params.list.directory = arg;
		break;
	case 'l':
		arguments->params.list.long_format = true;
		break;
	case 'a':
		arguments->params.list.show_hidden = true;
		break;
	case 'h':
		arguments->params.list.human_readable = true;
		break;
	case ARGP_KEY_ARG:
		if (!arguments->params.list.directory) {
			arguments->params.list.directory = arg;
		} else {
			argp_error(state, "多余参数: %s", arg);
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp list_argp = {
	list_options,
	parse_list_opt,
	"[目录]",
	"列出目录内容",
};

/*** MOVE 子命令解析器 ***/
static struct argp_option move_options[] = {
	{ "source", 's', "PATH", 0, "源文件/目录路径 (必需)" },
	{ "destination", 'd', "PATH", 0, "目标路径 (必需)" },
	{ "interactive", 'i', 0, 0, "覆盖前提示确认" },
	{ "update", 'u', 0, 0, "仅当源文件较新时移动" },
	{ 0 },
};

static error_t parse_move_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key) {
	case 's':
		arguments->params.move.source = arg;
		break;
	case 'd':
		arguments->params.move.destination = arg;
		break;
	case 'i':
		arguments->params.move.interactive = true;
		break;
	case 'u':
		arguments->params.move.update = true;
		break;
	case ARGP_KEY_ARG:
		if (!arguments->params.move.source) {
			arguments->params.move.source = arg;
		} else if (!arguments->params.move.destination) {
			arguments->params.move.destination = arg;
		} else {
			argp_error(state, "多余参数: %s", arg);
		}
		break;
	case ARGP_KEY_END:
		if (!arguments->params.move.source) {
			argp_error(state, "必须提供源路径 (--source)");
		}
		if (!arguments->params.move.destination) {
			argp_error(state, "必须提供目标路径 (--destination)");
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp move_argp = {
	move_options,
	parse_move_opt,
	"[源路径] [目标路径]",
	"移动或重命名文件/目录",
};

/*** SEARCH 子命令解析器 ***/
static struct argp_option search_options[] = {
	{ "pattern", 'p', "TEXT", 0, "搜索模式 (必需)" },
	{ "location", 'l', "PATH", 0, "搜索位置 (默认: 当前目录)" },
	{ "case-sensitive", 'c', 0, 0, "区分大小写搜索" },
	{ "regex", 'r', 0, 0, "使用正则表达式" },
	{ 0 },
};

static error_t parse_search_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key) {
	case 'p':
		arguments->params.search.pattern = arg;
		break;
	case 'l':
		arguments->params.search.location = arg;
		break;
	case 'c':
		arguments->params.search.case_sensitive = true;
		break;
	case 'r':
		arguments->params.search.regex = true;
		break;
	case ARGP_KEY_ARG:
		if (!arguments->params.search.pattern) {
			arguments->params.search.pattern = arg;
		} else if (!arguments->params.search.location) {
			arguments->params.search.location = arg;
		} else {
			argp_error(state, "多余参数: %s", arg);
		}
		break;
	case ARGP_KEY_END:
		if (!arguments->params.search.pattern) {
			argp_error(state, "必须提供搜索模式 (--pattern)");
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp search_argp = {
	search_options,
	parse_search_opt,
	"[模式] [位置]",
	"搜索文件内容",
};

/*** STATS 子命令解析器 ***/
static struct argp_option stats_options[] = {
	{ "path", 'p', "PATH", 0, "文件/目录路径 (默认: 当前目录)" },
	{ "permissions", 'm', 0, 0, "显示权限信息" },
	{ "ownership", 'o', 0, 0, "显示所有者信息" },
	{ "timestamps", 't', 0, 0, "显示时间戳信息" },
	{ 0 },
};

static error_t parse_stats_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key) {
	case 'p':
		arguments->params.stats.path = arg;
		break;
	case 'm':
		arguments->params.stats.show_permissions = true;
		break;
	case 'o':
		arguments->params.stats.show_ownership = true;
		break;
	case 't':
		arguments->params.stats.show_timestamps = true;
		break;
	case ARGP_KEY_ARG:
		if (!arguments->params.stats.path) {
			arguments->params.stats.path = arg;
		} else {
			argp_error(state, "多余参数: %s", arg);
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp stats_argp = {
	stats_options,
	parse_stats_opt,
	"[路径]",
	"显示文件/目录统计信息",
};

/*** 子命令层级结构 ***/
static struct argp_child children[] = {
	{ &add_argp, 0, "add", 1 },
	{ &remove_argp, 0, "remove", 1 },
	{ &list_argp, 0, "list", 1 },
	{ &move_argp, 0, "move", 1 },
	{ &search_argp, 0, "search", 1 },
	{ &stats_argp, 0, "stats", 1 },
	{ 0 },
};

/*** 顶层解析器 ***/
static struct argp top_argp = {
	global_options,
	parse_global_opt,
	"COMMAND [OPTIONS...]",
	"文件管理工具 - 一个高级命令行文件管理器\n\n"
	"支持多种操作：添加、删除、移动文件，列出目录内容，搜索文件内容和查看文件统计信息。",
	children,
	0,
	0,
};

/*** 模拟命令执行函数 ***/
void execute_add_command(struct arguments *args)
{
	printf("\n执行添加操作:\n");
	printf("  名称: %s\n", args->params.add.name);
	printf("  类型: %s\n", args->params.add.type);
	if (args->params.add.content) {
		printf("  内容: %s\n", args->params.add.content);
	}
	printf("  覆盖: %s\n", args->params.add.overwrite ? "是" : "否");
	printf("  详细模式: %s\n", args->verbose ? "开启" : "关闭");
	printf("  递归模式: %s\n", args->recursive ? "开启" : "关闭");
}

void execute_remove_command(struct arguments *args)
{
	printf("\n执行删除操作:\n");
	printf("  目标: %s\n", args->params.remove.target);
	printf("  强制: %s\n", args->params.remove.force ? "是" : "否");
	printf("  保护根目录: %s\n",
	       args->params.remove.preserve_root ? "是" : "否");
	printf("  详细模式: %s\n", args->verbose ? "开启" : "关闭");
	printf("  递归模式: %s\n", args->recursive ? "开启" : "关闭");
}

void execute_list_command(struct arguments *args)
{
	printf("\n执行列表操作:\n");
	printf("  目录: %s\n", args->params.list.directory ?
				       args->params.list.directory :
				       "当前目录");
	printf("  长格式: %s\n", args->params.list.long_format ? "是" : "否");
	printf("  显示隐藏文件: %s\n",
	       args->params.list.show_hidden ? "是" : "否");
	printf("  人类可读大小: %s\n",
	       args->params.list.human_readable ? "是" : "否");
	printf("  详细模式: %s\n", args->verbose ? "开启" : "关闭");
}

void execute_move_command(struct arguments *args)
{
	printf("\n执行移动操作:\n");
	printf("  源: %s\n", args->params.move.source);
	printf("  目标: %s\n", args->params.move.destination);
	printf("  交互模式: %s\n", args->params.move.interactive ? "是" : "否");
	printf("  仅更新: %s\n", args->params.move.update ? "是" : "否");
	printf("  详细模式: %s\n", args->verbose ? "开启" : "关闭");
	printf("  递归模式: %s\n", args->recursive ? "开启" : "关闭");
}

void execute_search_command(struct arguments *args)
{
	printf("\n执行搜索操作:\n");
	printf("  模式: %s\n", args->params.search.pattern);
	printf("  位置: %s\n", args->params.search.location ?
				       args->params.search.location :
				       "当前目录");
	printf("  区分大小写: %s\n",
	       args->params.search.case_sensitive ? "是" : "否");
	printf("  使用正则: %s\n", args->params.search.regex ? "是" : "否");
	printf("  详细模式: %s\n", args->verbose ? "开启" : "关闭");
	printf("  递归模式: %s\n", args->recursive ? "开启" : "关闭");
}

void execute_stats_command(struct arguments *args)
{
	printf("\n执行统计操作:\n");
	printf("  路径: %s\n",
	       args->params.stats.path ? args->params.stats.path : "当前目录");
	printf("  显示权限: %s\n",
	       args->params.stats.show_permissions ? "是" : "否");
	printf("  显示所有者: %s\n",
	       args->params.stats.show_ownership ? "是" : "否");
	printf("  显示时间戳: %s\n",
	       args->params.stats.show_timestamps ? "是" : "否");
	printf("  详细模式: %s\n", args->verbose ? "开启" : "关闭");
}

/*** 主函数 ***/
int demo_argp_parse_3_main(int argc, char **argv)
{
	struct arguments arguments = {
		.cmd = CMD_NONE,
		.verbose = false,
		.recursive = false,
	};

	// 解析命令行参数
	argp_parse(&top_argp, argc, argv, 0, 0, &arguments);

	// 执行对应子命令
	switch (arguments.cmd) {
	case CMD_ADD:
		execute_add_command(&arguments);
		break;
	case CMD_REMOVE:
		execute_remove_command(&arguments);
		break;
	case CMD_LIST:
		execute_list_command(&arguments);
		break;
	case CMD_MOVE:
		execute_move_command(&arguments);
		break;
	case CMD_SEARCH:
		execute_search_command(&arguments);
		break;
	case CMD_STATS:
		execute_stats_command(&arguments);
		break;
	default:
		fprintf(stderr, "未知命令\n");
		return 1;
	}

	return 0;
}
