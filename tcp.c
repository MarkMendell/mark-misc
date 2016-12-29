#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


int
main()
{
	// Get host, port, and whether to reconnect
	int reconnect = 0;
	int hosti = 1;
	if (argc < 2) {
		fprintf(stderr, "usage: tcp [-r] host [port]\n");
		return EXIT_FAILURE;
	} else if ((argc > 2) && (!strcmp(argv[1], "-r"))) {
		reconnect = 1;
		hosti++;
	}
	char *host = argv[hosti];
	char *port;
	if (argc > hosti+1)
		port = argv[hosti+1];
	else
		port = "80";

	// Prep connection info and socket
	struct addrinfo *ainfo;
	struct addrinfo hints = { .ai_protocol = IPPROTO_TCP };
	int gaires = getaddrinfo(host, port, &hints, &ainfo);
	if (gaires) {
		fprintf(stderr, "tcp: getaddrinfo looking up '%s:%s': %s\n", host, port, gai_strerror(gaires));
		return EXIT_FAILURE;
	}
	int socketfd = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
	if (socketfd < 0)
		pexit("socket", &ainfo, NULL);

	// Read/write stdin -> socket -> stdout
	struct pollfd stdinpoll = { .fd = STDIN_FILENO };
	struct pollfd socketpoll = { .fd = socketfd };
	struct pollfd stdoutpoll = { .fd = STDOUT_FILENO };
	struct pollfd polls[3] = {stdinpoll, socketpoll, stdoutpoll};
	struct ringbuf stdbuf, sockbuf;
	int res;
CONNECT:
	if (connect(socketfd, ainfo->ai_addr, ainfo->ai_addrlen))
		pexit("connect", &ainfo, &socketfd);
	rbinit(&stdbuf);
	rbinit(&sockbuf);
	do {
		if (stdoutpoll.revents)
			rbwrite(sockbuf, STDOUT_FILENO);
		if (socketpoll.revents) {
			rbread(sockbuf, socketfd);
			rbwrite(stdbuf, socketfd);
		}
		if (stdinpoll.revents)
			rbread(stdbuf, STDIN_FILENO);
		stdinpoll.revents = POLLIN * (!rbfull(&stdbuf) && !stdbuf.reof);
		socketpoll.revents = POLLERR | (POLLHUP * !(stdbuf.weof || sockbuf.reof));
		socketpoll.revents |= (POLLOUT * !rbempty(&stdbuf)) | (POLLIN * !rbfull(&sockbuf));
		stdoutpoll.revents = POLLERR | (POLLHUP * !sockbuf.weof) | (POLLOUT * !rbempty(&sockbuf));
		rberrordie(&stdbuf, socketfd);
		rberrordie(&sockbuf, socketfd);
		if ((stdbuf.weof || sockbuf.reof) && rbempty(&sockbuf)) {
			if (!rbempty(&stdbuf)) {
				fprintf(stderr, "tcp: %u bytes not sent\n", rbsize(&stdbuf));
				close(socketfd);
				return EXIT_FAILURE;
			} else if (reconnect) {
				stdinpoll.revents = POLLERR | POLLIN;
				while (((res = poll(&stdinpoll, 1, -1)) == -1) && ((errno == EINTR) || (errno == EAGAIN)));
				if (res == -1)
					pexit("poll", NULL, &socketfd);
				else if (stdinpoll.revents & POLLERR) {
					getchar();
					pexit("read stdin", &ainfo, &socketfd);
				}
				goto CONNECT;
			} else
				break;
		}
	} while (((res = poll(polls, 3, -1)) != -1) || (errno == EINTR) || (errno == EAGAIN));
	if (res == -1)
		pexit("poll", NULL, &socketfd);
	freeaddrinfo(ainfo);
	close(socketfd);
}
