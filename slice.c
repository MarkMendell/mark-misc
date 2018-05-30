#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


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

ssize_t
get(int fd, char *buf, unsigned long long n)
{
	ssize_t r;
	do r=read(fd, buf, (n > 4096) ? 4096 : n); while ((r==-1) && (errno==EINTR));
	if (r == -1)
		die("slice: read: %s", strerror(errno));
	if (!r)
		exit(0);
	return r;
}

int
main(int argc, char **argv)
{
	unsigned long long start, end;
	int fd = 0;
	if ((argc < 2) || (argc > 4))
		die("usage: slice end\n       slice start end [file]");
	else if (argc == 2) {
		start = 1;
		if (errno=0, end=strtoull(argv[1],NULL,10), errno)
			die("slice: bad end value '%s': %s", argv[1], strerror(errno));
	} else {
		if (errno=0, start=strtoull(argv[1],NULL,10), errno)
			die("slice: bad start value '%s': %s", argv[1], strerror(errno));
		if (errno=0, end=strtoull(argv[2],NULL,10), errno)
			die("slice: bad end value '%s': %s", argv[2], strerror(errno));
	}
	if (!start-- || !end)
		die("slice: values must be greater than zero");
	if (argc == 4) {
		if ((fd=open(argv[3], O_RDONLY)) == -1)
			die("slice: open '%s': %s", argv[3], strerror(errno));
		if (lseek(fd, start, SEEK_SET) == -1)
			die("slice: lseek: %s", strerror(errno));
	}
	char buf[4096];
	if (!fd) {
		unsigned long long i = 0;
		while (i < start)
			i += get(fd, buf, start-i);
	}
	unsigned long long i = start;
	while (i < end) {
		ssize_t r = get(fd, buf, end-i);
		fwrite(buf, 1, r, stdout);
		if (ferror(stdout))
			die("slice: fwrite: %s", strerror(errno));
		i += r;
	}
}
