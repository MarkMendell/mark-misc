#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void pexit(char *msg);
int getcharordie(void);
int gpchar(int print);
void nokeydie(char *key);
void eofdie(char *key);
void readobj(char *key, int print);
void readarr(char *skey, int print);
void readvalue(char *key, int print);
int main(int argc, char **argv);


void
pexit(char *msg)
{
	int olderrno = errno;
	fputs("jget: ", stderr);
	errno = olderrno;
	perror(msg);
	exit(EXIT_FAILURE);
}

int
getcharordie(void)
{
	int c = getchar();
	if (ferror(stdin))
		pexit("getchar");
	return c;
}

int
gpchar(int print)
{
	int c = getcharordie();
	if (print && (c != EOF) && (putchar(c) == EOF))
		pexit("putchar");
	return c;
}

void
nokeydie(char *key)
{
	fprintf(stderr, "jget: key '%s' not found\n", key);
	exit(EXIT_FAILURE);
}

void
eofdie(char *key)
{
	if (key)
		nokeydie(key);
	else
		fprintf(stderr, "jget: premature EOF\n");
	exit(EXIT_FAILURE);
}

void
readobj(char *key, int print)
{
	int c;
	while ((c = gpchar(print)) != '}') {
		if (c == ',')
			gpchar(print);  // "
		int escaped = 0;
		int keymatch = key != NULL;
		int keylen = key ? strlen(key) : -1;
		int i = 0;
		while (((c = gpchar(print)) != EOF) && ((c != '"') || escaped)) {
			escaped = (c == '\\') && !escaped;
			keymatch = keymatch && (i < keylen) && (key[i++] == c);
		}
		if (c == EOF)
			eofdie(key);
		gpchar(print);  // :
		if (keymatch && (i == keylen))
			break;
		readvalue(NULL, print);
	}
	if (key && (c == '}'))
		nokeydie(key);
}

void
readarr(char *skey, int print)
{
	int key = skey ? atoi(skey) : -1;
	int i = 0;
	int c;
	while (((c = getcharordie()) != ']') && (c != EOF)) {
		if (c != ',') {
			if (ungetc(c, stdin) == EOF)
				pexit("ungetc");
		} else if (print && (putchar(c) == EOF))
			pexit("putchar");
		if (key == i)
			break;
		readvalue(NULL, print);
		i++;
	}
	if ((c == ']') && print && (putchar(c) == EOF))
		pexit("putchar");
	if (skey && (key != i)) {
		nokeydie(skey);
	}
}

void
readvalue(char *key, int print)
{
	int c = getcharordie();
	if (print && (c != '"') && (c != EOF) && (putchar(c) == EOF))
		pexit("putchar");
	if (c == '{')
		readobj(key, print);
	else if (c == '[')
		readarr(key, print);
	else if (key || (c == EOF))
		eofdie(key);
	else if (c == '"') {
		int escaped = 0;
		while (((c = getcharordie()) != EOF) && ((c != '"') || escaped)) {
			if (print && (putchar(c) == EOF))
				pexit("putchar");
			escaped = (c == '\\') && !escaped;
		}
		if (c == EOF)
			eofdie(key);
	} else {
		while (((c = getcharordie()) != EOF) && (c != ',') && (c != '}') && (c != ']'))
			if (print && (putchar(c) == EOF))
				pexit("putchar");
		if ((c != EOF) && (ungetc(c, stdin) == EOF))
			pexit("ungetc");
	}
}

int
main(int argc, char **argv)
{
	for (int i=1; i<=argc; i++)
		readvalue((i == argc) ? NULL : argv[i], i == argc);
}
