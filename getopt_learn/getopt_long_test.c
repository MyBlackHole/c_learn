#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

static struct option sub_cmd[] = {
	{ "backup", no_argument, NULL, 3000 },
	{ "backup-merge", no_argument, NULL, 3001 },
	{ "restore", no_argument, NULL, 3002 },
	{ "restore-size", no_argument, NULL, 3003 },
	{ "backup-test", required_argument, NULL, 3004 },
	{ 0, 0, 0, 0 },
};

static int sub_cmd_process(const int argc, char **argv,
			   struct option *long_options)
{
	int c = 0;
	int long_index = 0;

	printf("sub_cmd_process\n");
	printf("argc: %d\n", argc);

	while ((c = getopt_long(argc, argv, "", long_options, &long_index)) !=
	       -1) {
		switch (c) {
		case 3000: {
			printf("backup\n");
			break;
		}
		case 3001: {
			printf("backup-merge\n");
			break;
		}
		case 3002: {
			printf("restore\n");
			break;
		}
		case 3003: {
			printf("restore-size\n");
			break;
		}
		case 3004: {
			printf("restore-size\n");
			break;
		}
		default: {
			fprintf(stderr, "bad opt: %d\n", c);
			exit(1);
		}
		}
		printf("optarg: %s\n", optarg);
		printf("optind: %d\n", optind);
	}

	optind = 0;
	return 0;
}

int demo_getopt_long_test_main(int argc, char *argv[])
{
	sub_cmd_process(argc, argv, sub_cmd);
	return EXIT_SUCCESS;
}
