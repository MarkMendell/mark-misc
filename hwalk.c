#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Length of the below list */
#define VOIDLEN 16

/* Void tags (automatically self-closing) */
char VOID[VOIDLEN][9] = {
	"area",
	"base",
	"br",
	"col",
	"doctype",
	"embed",
	"hr",
	"img",
	"input"
	"link",
	"menuitem",
	"meta",
	"param",
	"source",
	"track",
	"wbr"
};


/* Outputs errno's error message preceded by "hwalk: <msg>: ", closes f, and exits with nonzero
 * status. */
void
pexit(char *msg, FILE *f)
{
	int olderrno = errno;
	fputs("hwalk: ", stderr);
	errno = olderrno;
	perror(msg);
	fclose(f);
	exit(EXIT_FAILURE);
}

/* Seek to i in f, terminating if there's an error. */
void
fseekordie(FILE *f, long i)
{
	if (fseek(f, i, 0))
		pexit("fseek", f);
}

/* Get the file position of f, terminating if there's an error. */
long
ftellordie(FILE *f)
{
	long i = ftell(f);
	if (i == -1)
		pexit("ftell", f);
	return i;
}

/* Get the next character or EOF from f and output it, terminating if there's an error. */
int
getputcordie(FILE *f)
{
	int c = fgetc(f);
	if (c == EOF) {
		if (ferror(f))
			pexit("fgetc from tempfile", f);
	} else if (putchar(c) == EOF)
		pexit("putchar", f);
	return c;
}

/* Get and output the next character from f, terminating if EOF is gotten. */
int
requireputc(FILE *f)
{
	int c = getputcordie(f);
	if (c == EOF) {
		fprintf(stderr, "hwalk: premature EOF while parsing\n");
		fclose(f);
		exit(EXIT_FAILURE);
	}
	return c;
}

int
main(void)
{
	// Save input to temporary file (so we can seek)
	FILE *f;
	while (((f = tmpfile()) == NULL) && (errno == EINTR));
	if (f == NULL)
		pexit("tmpfile", f);
	int c;
	while ((c = getchar()) != EOF)
		if (fputc(c, f) == EOF)
			pexit("fputc to tempfile", f);
	if (ferror(stdin))
		pexit("getchar", f);
	fseekordie(f, 0);

	// Walk tags
	long namestart, nextelem;
	int closing, prevc, depth;
OUTSIDETAG:
	// Read until tag
	while (!(((c = fgetc(f)) == EOF) || ((c == '<') && (((c = fgetc(f)) == EOF) || isalnum(c)))));
	if (c == EOF) {
		if (ferror(f))
			pexit("fgetc from tempfile", f);
		else {
			fclose(f);
			return EXIT_SUCCESS;
		}
	}
	puts("found tag");
	namestart = ftellordie(f) - 1;
	// Output the part that we just read
	if ((putchar('<') == EOF) || (putchar(c) == EOF))
		pexit("putchar", f);
	// Output every tag within the tag we found
	nextelem = -1;
	closing = 0;
	depth = 0;
	while ((c = getputcordie(f)) != EOF) {
		// Start of tag
		if (c == '<')
			// <!-- comment -->
			if ((c = requireputc(f)) == '!')
				while ((requireputc(f) != '-') || (requireputc(f) != '-') || (requireputc(f) != '>'));
			// </closing tag>
			else if (c == '/')
				closing = 1;
			// <opening tag>
			else {
				closing = 0;
				namestart = ftellordie(f) - 1;
				if (nextelem == -1)
					nextelem = namestart - 1;
			}
		// End of tag
		else if (c == '>') {
			// </closing tag>
			if (closing)
				depth -= 1;
			// <opening tag>
			else {
				// Read name of tag that just finished
				unsigned int taglen = ftellordie(f) - 1 - namestart;
				fseekordie(f, namestart);
				char tag[taglen+1];
				if (fread(tag, sizeof(char), taglen+1, f) != taglen+1)
					pexit("fread from tempfile", f);
				tag[taglen] = '\0';
				// Check if tag is selfclosing (either ended with '/' or void)
				int selfclosing = (prevc == '/');
				for (int i=0; i<VOIDLEN; i++)
					selfclosing = selfclosing || !strcasecmp(tag, VOID[i]);
				depth += !selfclosing;
			}
			// Finished current tag
			if (depth == 0) {
				// Finished entire outer tag
				if (nextelem == -1)
					{ puts("finished outer tag"); break;
					goto OUTSIDETAG;
					}
				// Still recursing inside outer tag
				else
					{ puts("still recursing"); break;
					fseekordie(f, nextelem);
					}
			}
			namestart = -1;
		}
		prevc = c;
	}
	fclose(f);
}
