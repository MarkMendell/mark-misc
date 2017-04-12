#include <stdio.h>


int
main()
{
	int c;
	while ((c = getchar()) != EOF) {
		if (c == '"') {
			int escaped = 0;
			while (((c = getchar()) != EOF) && ((c != '"') || escaped))
		}
