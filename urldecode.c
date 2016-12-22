#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


/* Write errno's corresponding error message preceded by "urldecode: <msg>: ", then exit with
 * nonzero status. */
void
pexit(char *msg)
{
	int olderrno = errno;
	fputs("urldecode: ", stderr);
	errno = olderrno;
	perror(msg);
	exit(EXIT_FAILURE);
}

/* If stdin has an error, output the appropriate error message and exit with nonzero status (assumes
 * errno has just been set for getchar). Otherwise, return 0. */
int
ferrordie(void)
{
	if (ferror(stdin))
		pexit("getchar");
	return 0;
}

/* Write msg to stderr preceded by "urldecode: " and followed by a newline, then exit with nonzero
 * status. */
int
die(char *msg)
{
	fprintf(stderr, "urldecode: %s\n", msg);
	exit(EXIT_FAILURE);
	return 0;
}

/* Return c's value interpreted as a hex digit, exiting with nonzero status if it isn't one. */
uint8_t
hexordie(char c)
{
	c = toupper(c);
	if (isdigit(c))
		return c - '0';
	else if ((c < 'A') || (c > 'F'))
		die("invalid hex digit");
	return (c - 'A') + 0xA;
}

int
main(void)
{
	char c;
	while ((c = getchar()) != EOF) {
		if (c == '%') {
			char x1, x2;
			if (((x1 = getchar()) == EOF) || ((x2 = getchar()) == EOF))
				ferrordie() || die("EOF before end of %-encoded value");
			c = hexordie(x1)<<4 | hexordie(x2);
		}
		if (putchar(c) == EOF)
			pexit("putchar");
	}
	ferrordie();
}
