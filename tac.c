#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


void
pexit(char *msg, char *buf)
{
	int olderrno = errno;
	fputs("tac: ", stderr);
	errno = olderrno;
	perror(msg);
	free(buf);
	exit(EXIT_FAILURE);
}

int
main(void)
{
	size_t capacity = 4096;
	size_t len = 0;
	char *buf = malloc(capacity);
	if (!buf)
		pexit("malloc", NULL);
	while (!feof(stdin)) {
		if ((capacity + 4096*8) <= 4096*8) {
			fputs("tac: input too long\n", stderr);
			free(buf);
			return EXIT_FAILURE;
		}
		if (((capacity - len) < 4096) && ((buf = realloc(buf, capacity += 4096*8)) == NULL))
			pexit("realloc", buf);
		len += fread(&buf[len], 1, 4096, stdin);
		if (ferror(stdin))
			pexit("fread", buf);
	}
	if (len == 0)
		return EXIT_SUCCESS;
	size_t end = (buf[len-1] == '\n') ? len-1 : len;
	while ((end != 0) && (end <= len)) {
		size_t start = end;
		while ((start != 0) && (buf[start-1] != '\n'))
			start--;
		fwrite(&buf[start], 1, end-start, stdout);
		if (ferror(stdout))
			pexit("fwrite", buf);
		if (putchar('\n') == EOF)
			pexit("putchar", buf);
		end = start - 1;
	}
	if ((buf[0] == '\n') && (putchar('\n') == EOF))
		pexit("putchar", buf);
}
