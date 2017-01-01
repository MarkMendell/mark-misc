#include <stdio.h>
#include <stdlib.h>


int
main(void)
{
	int c;
	int escaped = 0;
	while ((c = getchar()) != EOF) {
		if (escaped)
			switch(c) {
				case 'b':
					putchar('\b');
					break;
				case 'f':
					putchar('\f');
					break;
				case 'n':
					putchar('\n');
					break;
				case 'r':
					putchar('\r');
					break;
				case 't':
					putchar('\t');
					break;
				// TODO: \u91AFBBA4
				default:
					putchar(c);
					break;
			}
		else if (c != '\\')
			putchar(c);
		if (ferror(stdout)) {
			perror("putchar");
			return EXIT_FAILURE;
		}
		escaped = (c == '\\') && !escaped;
	}
	if (ferror(stdin)) {
		perror("getchar");
		return EXIT_FAILURE;
	}
}
