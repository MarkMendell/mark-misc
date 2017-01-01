#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int
main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: strcmp s1 s2\n");
		return EXIT_FAILURE;
	}
	printf("%d\n", strcmp(argv[1], argv[2]));
}
