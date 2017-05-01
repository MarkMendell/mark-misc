#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
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


void
say(char type, char *tag, char *body, size_t len)
{
	char msg[7+len];
	((uint32_t*)msg)[0] = sizeof(msg);
	msg[4] = type;
	memcpy(msg+5, tag, 2);
	memcpy(msg+7, body, len);
	size_t w = fwrite(msg, 1, sizeof(msg), stdout);
	if (ferror(stdout))
		die("9ps: fwrite: %s", strerror(errno));
	if (w < sizeof(msg))
		exit(EXIT_SUCCESS);
}

void
sayerr(char *tag, char *errfmt, ...)
{
	char err[256];
	va_list argp;
	va_start(argp, errfmt);
	va_end(argp);
	vsnprintf(err, 256, errfmt, argp);
	say(107, tag, err, strlen(err));
}

void
get(char *buf, size_t len, char *tag)
{
	size_t r = fread(buf, 1, len, stdin);
	if (ferror(stdin)) {
		char *err = strerror(errno);
		if (tag)
			sayerr(tag, "Error while reading from client: %s", err);
		die("9ps: fread: %s", err);
	}
	if (r < len) {
		if (tag)
			sayerr(tag, "Short read from client - expected %d, got %d", len, r);
		exit(EXIT_SUCCESS);
	}
}


int
main(int argc, char **argv)
{
	char hdr[7];
	while (1) {
		get(hdr, 7, NULL);
		sayerr(hdr+5, "Unknown message type %d", hdr[4]);
	}
}
