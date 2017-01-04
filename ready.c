#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int
main(void)
{
	struct pollfd inpoll = {
		.fd = STDIN_FILENO,
		.events = POLLIN
	};
	int res;
	//TODO: POLLERR POLLHUP?
	while (((res = poll(&inpoll, 1, -1)) == -1) && ((errno == EINTR) || (errno == EAGAIN)));
	if (res == -1) {
		perror("ready: poll");
		return EXIT_FAILURE;
	}
}
