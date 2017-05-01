#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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

int
main(int argc, char **argv)
{
	if (argc < 2)
		die("usage: tcpl port cmd [arg ...]");

	// Get matching network addresses
	struct addrinfo hints = {
		.ai_flags = AI_PASSIVE,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	struct addrinfo *addrs;
	int res = getaddrinfo(NULL, argv[1], &hints, &addrs);
	if (res)
		die("tcpl: getaddrinfo: %s", gai_strerror(res));

	// Bind and listen on the first address that works
	int fdl;
	for (; addrs; addrs=addrs->ai_next) {
		fdl = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
		if ((fdl != -1) && (bind(fdl, addrs->ai_addr, addrs->ai_addrlen) != -1))
			break;
	}
	if (!addrs)
		die("tcpl: couldn't bind socket to any address");
	if (listen(fdl, SOMAXCONN))
		die("tcpl: listen: %s", strerror(errno));

	// Concatenate cmd and args together
	int cmdlen = 0;
	for (int i=2; i<argc; i++)
		cmdlen += strlen(argv[i]) + 1;
	char cmd[cmdlen];
	cmd[0] = '\0';
	for (int i=2; i<argc; i++) {
		strcat(cmd, argv[i]);
		if (i != argc-1)
			strcat(cmd, " ");
	}

	// Run the command for each connection with stdin/out attached
	struct sockaddr addrc;
	int fdc;
	socklen_t addrclen;
	while (((fdc = accept(fdl, &addrc, &addrclen)) != -1) || (errno == EINTR)) {
		if (fdc == -1)
			continue;
		if ((res = fork()) == -1)
			die("tcpl: fork: %s", strerror(errno));
		if (res)
			close(fdc);
		else {
			dup2(dup2(fdc, 0), 1);
			close(fdl);
			if (execlp("sh", "sh", "-c", cmd, NULL))
				die("tcpl: execlp: %s", strerror(errno));
		}
	}
	die("tcpl: accept: %s", strerror(errno));
}
