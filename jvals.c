#include <stdio.h>
#include <stdlib.h>


int
getcharordie(void)
{
	int c = getchar();
	if ((c == EOF) || (c == '\n')) {
		int err = ferror(stdin);
		if (err)
			perror("jvals: getchar");
		exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
	}
	return c;
}

void
putcharordie(char c)
{
	if (putchar(c) == EOF) {
		perror("jvals: putchar");
		exit(EXIT_FAILURE);
	}
}

void
readstring(int print)
{
	int escaped = 0;
	int c;
	while (((c = getcharordie()) != '"') || escaped) {
		if (print)
			putcharordie(c);
		escaped = (c == '\\') && !escaped;
	}
}

int
main(void)
{
	int haskeys = getcharordie() == '{';
	while (1) {
		if (haskeys) {
			getcharordie();  // "
			readstring(0);
			getcharordie();  // :
		}
		int depth = 0;
		int c;
		while ((((c = getcharordie()) != ',') && (c != '}') && (c != ']')) || depth) {
			depth += ((c == '{') || (c == '[')) - ((c == '}') || (c == ']'));
			if (c == '"') {
				if (depth)
					putcharordie('"');
				readstring(1);
				if (depth)
					putcharordie('"');
			} else
				putcharordie(c);
		}
		putcharordie('\n');
	}
}
