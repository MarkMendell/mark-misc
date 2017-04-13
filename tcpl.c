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
	// Get interface if provided
	if ((argc < 2) || (!strcmp(argv[1], "-i") && (argc < 4)))
		die("usage: tcpl [-i interface] port cmd [arg ...]");
	char *interface = strcmp(argv[1], "-i") ? NULL : argv[2];
	int portargi = interface ? 3 : 1;

	// Get matching network addresses
	struct addrinfo hints = {
		.ai_flags = AI_PASSIVE,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_canonname = interface
	};
	struct addrinfo *addrs;
	int res = getaddrinfo(NULL, argv[portargi], &hints, &addrs);
	if (res)
		die("tcpl: getaddrinfo: %s", gai_strerror(res));

	// Bind and listen on the first address that works
	int fdl;
	for (; addrs; addrs=addrs->ai_next) {
		printf("%d\n", addrs->ai_flags & AI_CANONNAME);
		continue;
		if (interface && strcmp(addrs->ai_canonname, interface))
			continue;
		char *f = "socket";
		fdl = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
		if ((fdl == -1) ||
				(f="bind", bind(fdl, addrs->ai_addr, addrs->ai_addrlen) == -1)) {
			if (interface)
				die("tcpl: %s: %s", f, strerror(errno));
			else
				continue;
		}
		break;
	}
	puts("done");
	exit(0);
	if (!addrs)
		die("tcpl: couldn't bind socket to any address");
	if (listen(fdl, SOMAXCONN))
		die("tcpl: listen: %s", strerror(errno));

	// Concatenate cmd and args together
	int cmdlen = 0;
	for (int i=portargi+1; i<argc; i++)
		cmdlen += strlen(argv[i]) + 1;
	char cmd[cmdlen];
	cmd[0] = '\0';
	for (int i=portargi+1; i<argc; i++) {
		strcat(cmd, argv[i]);
		if (i != argc-1)
			strcat(cmd, " ");
	}

	// Run the command for each connection with stdin/out attached
	struct sockaddr addrc;
	int addrclen, fdc;
	while (((fdc = accept(fdl, &addrc, &addrclen)) != -1) || (errno == EINTR)) {
		if (fdc == -1)
			continue;
		if ((res = fork()) == -1)
			die("tcpl: fork: %s", strerror(errno));
		if (res)
			close(fdc);
		else {
			dup2(fdc, 0), dup2(fdc, 1);
			close(fdl);
			if (execlp("sh", "sh", "-c", cmd, NULL))
				die("tcpl: execlp: %s", strerror(errno));
		}
	}
	die("tcpl: accept: %s", strerror(errno));
}
