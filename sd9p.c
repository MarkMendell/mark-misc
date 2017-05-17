#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int MINLEN = 256;
const int TVERSION = 100;


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
int2le(char *buf, int len, int d)
{
	for (int i=0; i<len; i++)
		buf[i] = d >> 8*i;
}

void
say(char type, char *buf, size_t bodylen)
{
	int2le(buf, 4, bodylen+7);
	buf[4] = type;
	size_t w = fwrite(buf, 1, bodylen+7, stdout);
	if (ferror(stdout))
		die("9ps: fwrite: %s", strerror(errno));
	if (w < bodylen+7)
		exit(EXIT_SUCCESS);
}

void
sayerr(char *buf, char *errfmt, ...)
{
	va_list argp;
	va_start(argp, errfmt);
	vsnprintf(buf+9, MINLEN-9, errfmt, argp);
	va_end(argp);
	size_t len = strlen(buf+9);
	int2le(buf+7, 2, len);
	say(107, buf, len+2);
}

void
get(char *dest, size_t len, char *buf)
{
	size_t r = fread(dest, 1, len, stdin);
	if (ferror(stdin)) {
		char *err = strerror(errno);
		sayerr(buf, "error reading input");
		die("9ps: fread: %s", err);
	}
	if (r < len) {
		if (r || (buf != dest))  // Ended in middle of a message
			sayerr(buf, "short read - expected at least %d, got %d", len, r);
		exit(EXIT_SUCCESS);
	}
}


int
le2int(char *buf, int len)
{
	int d = 0;
	for (int i=0; i<len; i++)
		d += buf[i] << 8*i;
	return d;
}

int
main(int argc, char **argv)
{
	setbuf(stdout, NULL);
	char *buf = malloc(MINLEN);
	if (!buf)
		die("sd9p: malloc first %d (?!) bytes: %s", MINLEN, strerror(errno));
	size_t len = 0;

	while (!feof(stdin)) {
		// Read in message
		buf[5]=buf[6]=0xFF, get(buf, 4, buf);
		int msglen = le2int(buf, 4);
		if (msglen < 7) {
			sayerr(buf, "stub message (shorter than 7 bytes? wtf?)");
			continue;
		}
		if (!len && (msglen > MINLEN)) {  // First message and longer than default
			if (!(buf = realloc(buf, msglen)))
				die("sd9p: realloc: %s", strerror(errno));
			buf[5]=buf[6]=0xFF, get(buf+4, msglen-4, buf);
		} else if (len && (msglen > len)) {  // Message too long
			get(buf+4, 3, buf);
			sayerr(buf, "message length %d > %d", msglen, len);
			do get(buf+7, len-7, buf); while ((msglen-=(len-7))-7 > len);
			get(buf+7, msglen-7, buf);
			continue;
		} else  // Message fits
			get(buf+4, msglen-4, buf);

		// First message must be version message
		if (!len && (buf[4] != TVERSION)) {
			sayerr(buf, "version message required");
			continue;
		}

		// Handle each message type
		switch(buf[4]) {
		case TVERSION:
			if ((msglen < 7+6) || (msglen < 7+6+le2int(buf+7+4, 2))) {
				sayerr(buf, "partial version message");
				continue;
			}
			int vlen = le2int(buf+7+4, 2);
			if ((vlen < 6) || strncmp(buf+7+6, "9P2000", 6) ||
					((vlen > 6) && (buf[7+6+6] != '.'))) {
				strncpy(buf+7+6, "unknown", 7);
				int2le(buf+7+4, 2, 7);
				say(TVERSION+1, buf, 4+2+7);
				continue;
			}
			if (le2int(buf+7, 4) < MINLEN) {
				sayerr(buf, "message size too small");
				continue;
			}
			if (!(buf = realloc(buf, le2int(buf+7, 4))))
				die("sd9p: realloc: %s", strerror(errno));
			len = le2int(buf+7, 4);
			int2le(buf+7+4, 2, 6);
			say(TVERSION+1, buf, 4+2+6);
			break;

		default:
			sayerr(buf, "unknown message type %d", buf[4]);
			break;
		}
	}
}
