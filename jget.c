#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void
die(char *errfmt, ...)
{
	va_list argp;
	va_start(argp, errfmt);
	vfprintf(stderr, errfmt, argp);
	va_end(argp);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

int
get(void)
{
	int c = getchar();
	if ((c == EOF) && ferror(stdin))
		die("jget: getchar: %s", strerror(errno));
	return c;
}

void
unget(char c)
{
	if (ungetc(c, stdin) == EOF)
		die("jget: ungetc: failed");
}

void
put(char c)
{
	if (putchar(c) == EOF)
		die("jget: putchar: %s", strerror(errno));
}

int
getput(int print)
{
	int c = get();
	if (print && (c != EOF))
		put(c);
	return c;
}

void
enddie(char *key)
{
	if (key)
		die("jget: key '%s' not found", key);
	else
		die("jget: premature EOF");
}

void readvalue(char *stopkey, int print, int depth);
void
readobj(char *stopkey, int print, int depth)
{
	int c;
	while ((c = getput(print)) != '}') {
		if (c == ',')
			getput(print);  // "
		int escaped = 0;
		int keymatch = stopkey != NULL;
		int keylen = stopkey ? strlen(stopkey) : -1;
		int i = 0;
		while (((c = getput(print)) != EOF) && ((c != '"') || escaped)) {
			escaped = (c == '\\') && !escaped;
			keymatch = keymatch && (escaped || ((i < keylen) && (stopkey[i++] == c)));
		}
		if (c == EOF)
			enddie(stopkey);
		getput(print);  // :
		if (keymatch && (i == keylen))
			break;
		readvalue(NULL, print, depth+1);
	}
	if (stopkey && (c == '}'))
		enddie(stopkey);
}

void
readarr(char *stopkey, int print, int depth)
{
	int istop = stopkey ? atoi(stopkey) : -1;
	int i = 0;
	int c;
	while (((c = get()) != ']') && (c != EOF)) {
		if (c != ',')
			unget(c);
		else if (print)
			put(c);
		if (istop == i)
			break;
		readvalue(NULL, print, depth+1);
		i++;
	}
	if (print && (c == ']'))
		put(c);
	if (stopkey && (istop != i))
		enddie(stopkey);
}

void
readvalue(char *stopkey, int print, int depth)
{
	int c = get();
	if (print && (c != EOF) && (depth || (c != '"')))
		put(c);
	if (c == '{')
		readobj(stopkey, print, depth+1);
	else if (c == '[')
		readarr(stopkey, print, depth+1);
	else if (stopkey || (c == EOF))
		enddie(stopkey);
	else if (c == '"') {
		int escaped = 0;
		while (((c = get()) != EOF) && ((c != '"') || escaped)) {
			if (print)
				put(c);
			escaped = (c == '\\') && !escaped;
		}
		if (c == EOF)
			enddie(stopkey);
		else if (print && depth)
			put(c);
	} else {
		while (((c = get()) != EOF) && !strchr(",}]", c))
			if (print)
				put(c);
		if (c != EOF)
			unget(c);
	}
}

int
main(int argc, char **argv)
{
	int c;
	while ((c = get()) != EOF) {
		unget(c);
		for (int i=1; i<=argc; i++)
			readvalue(argv[i], i == argc, 0);
		put('\n');
		while (((c = get()) != EOF) && (c != '\n'));
	}
}
