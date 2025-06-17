#include "yoc.h"
#include <stdio.h>
#include <stdlib.h>
void die(const char *msg) {
	term_switch_to_norm();
	perror(msg);
	fflush(stderr);
	printf("\r");
	exit(EXIT_FAILURE);
}
void *xmalloc(size_t size) {
	void *ptr = malloc(size);
	if (!ptr && size > 0)
		die("malloc");
	return ptr;
}
void *xrealloc(void *ptr, size_t size) {
	void *new_ptr = realloc(ptr, size);
	if (!new_ptr && size > 0)
		die("realloc");
	return new_ptr;
}
