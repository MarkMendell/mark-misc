#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int
rwordie(int i, void *buf, size_t count)
{
	size_t c = i ? write(STDOUT_FILENO, buf, count) : read(STDIN_FILENO, buf, count);
	if (c == -1) {
		if (errno == EPIPE)
			return EOF;
		else if ((errno == EAGAIN) || (errno == EINTR))
			return 0;
		else {
			perror(i ? "buf: write" : "buf: read");
			exit(EXIT_FAILURE);
		}
	}
	return c ? c : EOF;
}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fputs("usage: buf size\n", stderr);
		return EXIT_FAILURE;
	}
	unsigned int size = strtol(argv[1], NULL, 10);
	if (size == 0) {
		fputs("buf: size must be positive integer\n", stderr);
		return EXIT_FAILURE;
	}

	for (int i=0; i<2; i++) {
		int fd = i ? STDOUT_FILENO : STDIN_FILENO;
		int flags = fcntl(fd, F_GETFL);
		if (flags == -1) {
			perror(i ? "buf: fcntl F_GETFL stdout" : "buf: fcntl F_GETFL stdin");
			return EXIT_FAILURE;
		}
		if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
			perror(i ? "buf: fcntl F_SETFL stdout" : "buf: fcntl F_SETFL stdin");
			return EXIT_FAILURE;
		}
	}

	struct pollfd polls[2] = { { .fd = STDIN_FILENO }, { .fd = STDOUT_FILENO } };
	char buf[size];
	int stdineof = 0;
	int stdouteof = 0;
	int writei = 0;
	int readi = -1;
	int res;
	do {
		// i=0: read from stdin to buf; i=1: write from buf to stdout
		for (int i=0; i<2; i++) {
			int *fi = i ? &readi : &writei;
			if (*fi == -1)
				continue;
			int *otheri = i ? &writei : &readi;
			int end = (*otheri > *fi) ? *otheri : size;
			size_t fc = rwordie(i, &buf[*fi], end - *fi);
			if (fc == EOF) {
				*(i ? &stdouteof : &stdineof) = 1;
				fc = 0;
			}
			if (fc && (*otheri == -1))
				*otheri = *fi;
			*fi = (*fi + fc) % size;
			if (*fi == *otheri)
				*fi = -1;
		}
		if (stdouteof || (stdineof && (readi == -1)))
			break;
		polls[0].fd -= stdineof * (-1 - polls[0].fd);
		polls[0].events = POLLIN * (writei != -1);
		polls[1].events = POLLOUT * (readi != -1);
	} while (((res = poll(polls, 2, -1)) != -1) || (errno == EINTR) || (errno == EAGAIN));
	if (res == -1) {
		perror("buf: poll");
		return EXIT_FAILURE;
	}
}
