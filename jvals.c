#include <stdio.h>
#include <stdlib.h>


int
getcharordie(void)
{
	int c = getchar();
	if (c == EOF) {
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
	while (1) {
		int c = getcharordie();
		int haskeys = c == '{';
		while ((c != '}') && (c != ']')) {
			if (haskeys) {
				getcharordie();  // "
				readstring(0);
				getcharordie();  // :
			}
			int depth = 0;
			int empty = !haskeys;
			while ((((c = getcharordie()) != ',') && (c != '}') && (c != ']')) || depth) {
				empty = 0;
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
			if (!empty)
				putcharordie('\n');
		}
		getcharordie();  // newline
	}
}
