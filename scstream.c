#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
cleanupordie(FILE *fromsc, FILE *tosc, char *l)
{
	int err = 0;
	if (fromsc && fclose(fromsc) == EOF) {
		err = 1;
		perror("fclose fromsc");
	}
	if (tosc && fclose(tosc) == EOF) {
		err = 1;
		perror("fclose tosc");
	}
	free(l);
	if (err)
		exit(EXIT_FAILURE);
}

/* Show an error message for errno preceded by "scstream: <msg>: " and exit with nonzero status
 * after closing fromsc and tosc and freeing l. */
void
pexit(char *msg, FILE *fromsc, FILE *tosc, char *l)
{
	int olderrno = errno;
	fputs("scstream: ", stderr);
	errno = olderrno;
	perror(msg);
	cleanupordie(fromsc, tosc, l);
	exit(EXIT_FAILURE);
}

/* Write "scstream: <msg>\n" to stderr, close fromsc and tosc, free l, and exit with nonzero status. */
void
die(char *msg, FILE *fromsc, FILE *tosc, char *l)
{
	fprintf(stderr, "scstream: %s\n", msg);
	cleanupordie(fromsc, tosc, l);
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	// Open fd 3 and 4 as stream (should be read/write TLS connection to api-v2.soundcloud.com)
	fprintf(stderr, "opening files\n");
	FILE *fromsc = fdopen(3, "r");
	if (fromsc == NULL)
		pexit("fdopen 3 (should be TLS read connection)", fromsc, NULL, NULL);
	if (setvbuf(fromsc, NULL, _IONBF, 0))
		pexit("setvbuf read connection", fromsc, NULL, NULL);
	FILE *tosc = fdopen(4, "w");
	if (tosc == NULL)
		pexit("fdopen 4 (should be TLS write connection)", fromsc, tosc, NULL);
	if (setvbuf(tosc, NULL, _IONBF, 0))
		pexit("setvbuf write connection", fromsc, tosc, NULL);
	fprintf(stderr, "opened files\n");

	// Read OAuth token from stdin
	fprintf(stderr, "reading token\n");
	char oauthtoken[50];
	size_t len = fread(oauthtoken, 1, sizeof(oauthtoken), stdin);
	if (ferror(stdin))
		pexit("fread stdin", fromsc, tosc, NULL);
	else if (len == sizeof(oauthtoken))
		die("oauth token longer than expected", fromsc, tosc, NULL);
	fprintf(stderr, "read token\n");

	// Send HTTP request and read response
	char requesttemplate[] = 
		"GET /stream HTTP/1.1\r\n"
		"host: api-v2.soundcloud.com\r\n"
		"Authorization: OAuth %s\r\n"
		"\r\n";
	fprintf(stderr, "writing request\n");
	if (fprintf(tosc, requesttemplate, oauthtoken) < 0)
		pexit("fprintf request", fromsc, tosc, NULL);
	fprintf(stderr, "wrote request\n");
	char *header = NULL;
	size_t n;
	fprintf(stderr, "getting status\n");
	if ((len = getline(&header, &n, fromsc)) == -1)
		pexit("getline status", fromsc, tosc, header);
	else if (len < 12)  // HTTP/1.1 XXX
		die("status shorter than expected", fromsc, tosc, header);
	else if (strncmp(&header[9], "200", 3)) {
		fprintf(stderr, "scstream: soundcloud status: '%3s'\n", &header[9]);
		cleanupordie(fromsc, tosc, header);
		return EXIT_FAILURE;
	}
	fprintf(stderr, "got status\n");
	long contentlen = -1;
	size_t cllen = strlen("content-length:");
	fprintf(stderr, "getting headers\n");
	while (((len = getline(&header, &n, fromsc)) != -1) && (header[0] != '\r'))
		if ((len >= cllen+1) && (!strncasecmp(header, "content-length:", cllen)))
			contentlen = strtol(&header[cllen], NULL, 10);
	if (len == -1)
		pexit("getline header", fromsc, tosc, header);
	if (contentlen == -1)
		die("no content-length header", fromsc, tosc, header);
	fprintf(stderr, "geot headers headers\n");
	char response[contentlen];
	fprintf(stderr, "boutta read dat shit\n");
	if (fread(response, 1, sizeof(response), fromsc) != sizeof(response)) {
		if (ferror(fromsc))
			pexit("fread response", fromsc, tosc, header);
		else
			die("premature end of response", fromsc, tosc, header);
	}
	fprintf(stderr, "boutta write dat shit\n");
	fwrite(response, 1, contentlen, stdout);
	cleanupordie(fromsc, tosc, header);
}
