#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#define LIB_PATH \
	"/media/black/Data/Documents/c/build/linux/x86_64/debug/libadd.so"
// #define LIB_PATH "/root/libadd.so"

typedef int FUNC(int, int);

int demo_demo_main(int argc, char **argv)
{
	void *handle;
	char *error;
	FUNC *func = NULL;
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <path to libadd.so>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	handle = dlopen(argv[1], RTLD_LAZY);
	// handle = dlopen(LIB_PATH, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "%s\n", dlerror());
		exit(EXIT_FAILURE);
	}

	printf("Library loaded: %s\n", dlerror());

	func = (FUNC *)dlsym(handle, "add");

	error = dlerror();
	if (error != NULL) {
		fprintf(stderr, "%s\n", error);
		exit(EXIT_FAILURE);
	}
	printf("add: %d\n", (*func)(2, 4));

	dlclose(handle);
	exit(EXIT_SUCCESS);
}
