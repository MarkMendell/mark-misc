#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Return the next character in stdin (or EOF) while keeping it on the stream. */
int
peek(void)
{
	int c = getchar();
	if (c != EOF)
		ungetc(c, stdin);
	return c;
}

/* Write s to stderr preceded by "btls: " and followed by a newline, then exit with nonzero status.
 */
int
die(char *s)
{
	fprintf(stderr, "btls: %s\n", s);
	exit(EXIT_FAILURE);
	return 0;
}

/* If stream has an error, exit after showing the error message (this function assumes errno is set
 * appropriately for that stream error), otherwise return 0. */
int
ferrordie(FILE *stream, char *name)
{
	if (ferror(stream)) {
		int olderrno = errno;
		fputs("btls: ", stderr);
		errno = olderrno;
		perror(name);
		exit(EXIT_FAILURE);
	}
	return 0;
}

/* Assuming the next character is a digit, read a number followed by : and return it, exiting with
 * non-zero status if the number is larger than size_t's capacity, there is no trailing :, or a read
 * error occurs. */
size_t
getbslen(void)
{
	size_t len = 0;
	int c;
	while (((c = getchar()) != EOF) && (isdigit(c))) {
		len = (len * 10) + (c - '0');
		if (len < (len - (c - '0')))
			die("byte string length too large");
	}
	if (c != ':')
		ferrordie(stdin, "stdin") || die("expected ':' after byte string length");
	return len;
}

/* Read len bytes into s followed by a null byte. Exit with nonzero status if there is a read error
 * or the file ended before len bytes were read (a byte string of length len is expected). */
void
readbs(char *s, size_t len)
{
	if (fread(s, sizeof(char), len, stdin) != len)
		ferrordie(stdin, "stdin") || die("EOF before end of byte string");
	s[len] = '\0';
}

int
main(void)
{
	int c;
	while ((c = peek()) != EOF)
		switch(c) {
			case 'i':
				while ((c = getchar()) != 'e');
				break;
			case 'l':
			case 'd':
			case 'e':
				getchar();
				break;
			default:
				if (!isdigit(c))
					die("invalid bencode start");
				size_t len = getbslen();
				char s[len + 1];
				readbs(s, len);
				if (!strcmp(s, "path")) {
					if (getchar() != 'l')
						ferrordie(stdin, "stdin") || die("expected list after 'path' byte string");
					int first = 1;
					while (((c = peek()) != 'e')) {
						if ((c == EOF) || (!isdigit(c)))
							ferrordie(stdin, "stdin") || die("expected byte string in path list");
						if ((len = getbslen()) == 0)
							die("0-length file name");
						char part[len + 1];
						readbs(part, len);
						printf(first ? "%s" : "/%s", part);
						ferrordie(stdout, "stdout");
						first = 0;
					}
					getchar();
					if (first)
						die("empty path entry");
					else {
						putchar('\n');
						ferrordie(stdout, "stdout");
					}
				}
				break;
		}
	ferrordie(stdin, "stdin");
}
