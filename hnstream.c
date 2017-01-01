#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


unsigned int IDLEN = 8;


/* Close fromhn and tohn and free l, exiting with nonzero status if there's an error. */
void
cleanupordie(FILE *fromhn, FILE *tohn, char *l)
{
	int err = 0;
	if (fromhn && fclose(fromhn) == EOF) {
		err = 1;
		perror("fclose fromhn");
	}
	if (tohn && fclose(tohn) == EOF) {
		err = 1;
		perror("fclose tohn");
	}
	free(l);
	if (err)
		exit(EXIT_FAILURE);
}

/* Show an error message for errno preceded by "hnstream: <msg>: " and exit with nonzero status
 * after closing fromhn and tohn and freeing l. */
void
pexit(char *msg, FILE *fromhn, FILE *tohn, char *l)
{
	int olderrno = errno;
	fputs("hnstream: ", stderr);
	errno = olderrno;
	perror(msg);
	cleanupordie(fromhn, tohn, l);
	exit(EXIT_FAILURE);
}

/* Write "hnstream: <msg>\n" to stderr, close fromhn and tohn, free l, and exit with nonzero status. */
void
die(char *msg, FILE *fromhn, FILE *tohn, char *l)
{
	fprintf(stderr, "hnstream: %s\n", msg);
	cleanupordie(fromhn, tohn, l);
	exit(EXIT_FAILURE);
}

/* Save id to savepath, exiting with nonzero status if an error occurs. */
void
saveordie(char *id, char *savepath, FILE *fromhn, FILE *tohn)
{
	FILE *savefile = fopen(savepath, "w");
	if (savefile == NULL)
		pexit("fopen savefile (to write)", fromhn, tohn, NULL);
	fwrite(id, 1, IDLEN, savefile);
	if (ferror(savefile)) {
		int olderrno = errno;
		fclose(savefile);
		errno = olderrno;
		pexit("fwrite savefile", fromhn, tohn, NULL);
	} else if (fclose(savefile) == EOF)
		pexit("fclose savefile (after write)", fromhn, tohn, NULL);
}

/* Parse HTTP response headers from fromhn to get the content length, exiting with nonzero status if
 * there's an error or the status header isn't 200. */
long
readlenordie(FILE *fromhn, FILE *tohn)
{
	char *header = NULL;
	size_t n, len;
	if ((len = getline(&header, &n, fromhn)) == -1)
		pexit("getline status", fromhn, tohn, header);
	else if (len < 12)  // HTTP/1.1 XXX
		die("status shorter than expected", fromhn, tohn, header);
	else if (strncmp(&header[9], "200", 3)) {
		fprintf(stderr, "hnstream: server status: '%.3s'\n", &header[9]);
		cleanupordie(fromhn, tohn, header);
		return EXIT_FAILURE;
	}
	long contentlen = -1;
	size_t cllen = strlen("content-length:");
	while (((len = getline(&header, &n, fromhn)) != -1) && (header[0] != '\r'))
		if ((len >= cllen+1) && (!strncasecmp(header, "content-length:", cllen)))
			contentlen = strtol(&header[cllen], NULL, 10);
	if (len == -1)
		pexit("getline header", fromhn, tohn, header);
	if (contentlen == -1)
		die("no content-length header", fromhn, tohn, header);
	free(header);
	return contentlen;
}


int
main(int argc, char **argv)
{
	// Don't buffer stdout (otherwise might update latest when not actually read)
	if (setvbuf(stdout, NULL, _IONBF, 0))
		pexit("setvbuf stdout", NULL, NULL, NULL);

	// Open fd 3 and 4 as stream (should be read/write TLS connection to hacker-news.firebaseio.com)
	FILE *fromhn = fdopen(3, "r");
	if (fromhn == NULL)
		pexit("fdopen 3 (should be TLS read connection)", fromhn, NULL, NULL);
	if (setvbuf(fromhn, NULL, _IONBF, 0))
		pexit("setvbuf read connection", fromhn, NULL, NULL);
	FILE *tohn = fdopen(4, "w");
	if (tohn == NULL)
		pexit("fdopen 4 (should be TLS write connection)", fromhn, tohn, NULL);
	if (setvbuf(tohn, NULL, _IONBF, 0))
		pexit("setvbuf write connection", fromhn, tohn, NULL);

	// Read save file (for where we left off)
	char previd[IDLEN + 1];
	FILE *savefile;
	if ((argc > 1) && ((savefile = fopen(argv[1], "r")) != NULL)) {
		size_t previdlen = fread(previd, 1, sizeof(previd), savefile);
		if (ferror(savefile)) {
			int olderrno = errno;
			fclose(savefile);
			errno = olderrno;
			pexit("fread savefile", fromhn, tohn, NULL);
		} else if (fclose(savefile) == EOF)
			pexit("fclose savefile (after read)", fromhn, tohn, NULL);
		else if (previdlen != IDLEN)
			die("saved id unexpected length", fromhn, tohn, NULL);
	} else if ((argc > 1) && (errno != ENOENT))
		pexit("fopen savefile (to read)", fromhn, tohn, NULL);
	else
		memset(previd, '0', IDLEN);
	previd[sizeof(previd)-1] = '\0';

	while (1) {
		// Get top stories
		char gettemplate[] = "GET /v0/%s%.*s.json HTTP/1.1\r\nhost: hacker-news.firebaseio.com\r\n\r\n";
		if (fprintf(tohn, gettemplate, "topstories", 0, "") < 0)
			pexit("fprintf newstories", fromhn, tohn, NULL);
		long storieslen = readlenordie(fromhn, tohn);
		char stories[storieslen];
		size_t rlen = fread(stories, 1, sizeof(stories), fromhn);
		if (ferror(fromhn))
			pexit("fread stories", fromhn, tohn, NULL);
		else if (rlen != sizeof(stories))
			die("premature end of stories response", fromhn, tohn, NULL);

		// Print out new stories older than a day in order
		long i;
		int wroteentry = 0;
		while (1) {
			// Get oldest new story
			char *next = NULL;
			for (i=1; i<storieslen-IDLEN; i+=IDLEN+1)
				if ((strncmp(&stories[i], previd, IDLEN) > 0) &&
						(!next || (strncmp(&stories[i], next, IDLEN) < 0)))
					next = &stories[i];
			if (next == NULL)
				break;
			// Get time of story
			if (fprintf(tohn, gettemplate, "item/", IDLEN, next) < 0)
				pexit("fprintf item", fromhn, tohn, NULL);
			long itemlen = readlenordie(fromhn, tohn);
			char storyinfo[itemlen];
			rlen = fread(storyinfo, 1, sizeof(storyinfo), fromhn);
			if (ferror(fromhn))
				pexit("fread story info", fromhn, tohn, NULL);
			else if (rlen != itemlen)
				die("premature end of item response", fromhn, tohn, NULL);
			unsigned long t = 0;
			for (long i=0; i<itemlen-7; i++) {  // 7 = length of "time":
				if (!strncmp(&storyinfo[i], "\"time\":", 7)) {
					for (i+=7; (i<itemlen) && (isdigit(storyinfo[i])); i++)
						t = t*10 + (t - '0');
					break;
				}
			}
			// Wait until story is at least a day old
			if (t == 0)
				die("no time in story info", fromhn, tohn, NULL);
			long wait = (t + 60*60*24) - time(NULL);
			if (wait > 0)
				sleep(wait);
			printf("%.*s\n", IDLEN, next);
			if (ferror(stdout))
				pexit("printf", fromhn, tohn, NULL);
			// Wait for signal that message was read (>tfw no unbuffered writes)
			ssize_t c, n;
			while (((n=read(5,&c,1))!=1)&&!((n==-1)&&!((errno==EAGAIN)||(errno==EINTR))));
			if (n == -1)
				pexit("read 5", fromhn, tohn, NULL);
			strncpy(previd, next, IDLEN);
			if (argc > 1)
				saveordie(next, argv[1], fromhn, tohn);
			wroteentry = 1;
		}
		if (!wroteentry)
			sleep(60*60*6);  // No new stories, so sleep for a while before pinging again
	}
}
