#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAXNAME 31
enum type { NUM, STR, PHRASE };
const char *SPECIAL = "\t\n(){}.$:";


struct buffer {
	char *mem;
	unsigned int len, cap;
};

struct num {
	unsigned int val;
	char isconst;
	char format;
	uint8_t len;
};

struct str {
	unsigned int lenstorage;
	unsigned int *plen;
};

struct phrase {
	char name[MAXNAME+1];
	struct wordnode *firsts;
	unsigned int nfirsts;
};

union typedata {
	struct num n;
	struct str s;
	struct phrase *p;
};

struct wordnode {
	union typedata t;
	char name[MAXNAME+1];
	enum type type;
	unsigned int ntimesstorage;
	unsigned int *pntimes;
	struct wordnode *nexts;
	unsigned int nnexts;
};

struct parsenode {
	int *pn;
	struct wordnode **pwords;
	struct wordnode *w;
	struct parsenode *prev;
};


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
getlabelc(FILE *f, int *pline, int *pcol)
{
	int c = fgetc(f);
	if ((c == EOF) && ferror(f))
		die("li: fgetc labels: %s", strerror(errno));
	if (!c)
		die("li: null byte in labels file");
	if (c == '\n')
		++*pline;
	if (c != EOF)
		++*pcol;
	return c;
}

int
getname(FILE *f, char *buf, char first, int *pline, int *pcol)
{
	int i = 0;
	if (first)
		buf[i++] = first;
	while (((buf[i++]=getlabelc(f,pline,pcol))!=EOF) && !strchr(SPECIAL, buf[i-1]))
		if (i == MAXNAME+1)
			die("li: name at line %d, col %d too long", *pline, *pcol);
	if (i == 1)
		die("li: expected name at line %d, col %d", *pline, *pcol);
	int c = buf[i-1];
	buf[i-1] = 0;
	return c;
}

int
isdigits(char *s)
{
	for (; *s; s++)
		if (!isdigit(*s))
			return 0;
	return 1;
}

unsigned int*
getpnum(FILE *f, int *pc, int *pline, int *pcol, struct parsenode *panode,
		struct parsenode *pahead)
{
	char buf[MAXNAME+1];
	*pc = getname(f, buf, 0, pline, pcol);
	for (; panode!=pahead; panode=pahead->prev)
		if (!strcmp(buf, panode->w->name)) {
			if (panode->w->type != NUM)
				die("li: name before line %d, col %d isn't a number type", *pline, *pcol);
			return &panode->w->t.n.val;
		}
	die("li: name before line %d, col %d not found", *pline, *pcol);
}

int
fillbuf(struct buffer *buf, unsigned int len)
{
	if (buf->len >= len)
		return 0;
	if (len > buf->cap) {
		while (len > (buf->cap*=2));
		if (!(buf->mem=realloc(buf->mem, buf->cap)))
			die("li: realloc buf %u: %s", buf->cap, strerror(errno));
	}
	buf->len += fread(buf->mem+buf->len, 1, len-buf->len, stdin);
	if (ferror(stdin))
		die("li: fread stdin: %s", strerror(errno));
	return buf->len >= len;
}

int
ismatch(struct buffer *buf, struct wordnode *w)
{
	switch(w->type) {
	case NUM: {
		if (fillbuf(buf, w->t.n.len))
			return 0;
		int val = 0;
		for (int i=0; i<w->t.n.len; i++)
			val |= buf->mem[i] << i*8;
		if (w->t.n.isconst && (val != w->t.n.val))
			return 0;
		w->t.n.val = val;
		return 1;
	} case STR:
		return !fillbuf(buf, *w->t.s.plen);
	case PHRASE:
		for (int i=0; i<w->t.p->nfirsts; i++)
			if (ismatch(buf, w->t.p->firsts+i))
				return 1;
		return 0;
	}
}

void
shiftbuf(struct buffer *buf, unsigned int len)
{
	memmove(buf->mem, buf->mem+len, len);
	buf->len -= len;
}

void
labelmsg(struct buffer *buf, struct wordnode *msgs, unsigned int nmsgs,
		struct buffer *prefix)
{
	struct wordnode *wnodes = msgs;
	unsigned int nwnodes = nmsgs;
	unsigned int iwnodes = 0;
	while (iwnodes < nwnodes) {
		struct wordnode *w = wnodes+iwnodes;
		unsigned int ntimes = *w->pntimes;
		for (unsigned int itimes=0; itimes<ntimes; itimes++) {
			if (!ismatch(buf, w)) {
				if (itimes) {
					fprintf(stderr, "li: expected %d, got %d\n", ntimes, itimes);
					goto bytes;
				}
				iwnodes++;
				goto nextnode;
			}
			int print = *w->name && (!prefix->len || (prefix->mem[prefix->len-1]!='\n'));
			if (w->type == PHRASE) {
				unsigned int oldlen = prefix->len;
				unsigned int oldend = oldlen && prefix->mem[oldlen-1];
				if (print) {
					prefix->len += strlen(w->name) + !!oldlen;
					if ((prefix->cap < prefix->len) &&
							!(prefix->mem=realloc(prefix->mem, prefix->cap +=MAXNAME+1)))
						die("li: realloc prefix %u: %s", prefix->cap, strerror(errno));
					if (oldlen)
						prefix->mem[oldlen] = '.';
					strcpy(prefix->mem+oldlen+1, w->name);
				} else
					prefix->mem[(prefix->len=prefix->len||1)-1] = '\n';
				labelmsg(buf, w->t.p->firsts, w->t.p->nfirsts, prefix);
				prefix->len = oldlen;
				if (oldlen)
					prefix->mem[oldlen-1] = oldend;
				continue;
			}
			if (print && (printf("%.*s%s\t", prefix->len, prefix->mem, w->name) < 0))
				die("li: printf: %s", strerror(errno));
			switch (w->type) {
			case NUM:
				if (print) {
					if (w->t.n.format == 'b') {
						for (unsigned int i=0; i<w->t.n.len; i++)
							if (putchar('0'+((w->t.n.val>>i)&1)) == EOF)
								die("li: putchar: %s", strerror(errno));
					} else if (printf("%u", w->t.n.val) < 0)
						die("li: printf: %s", strerror(errno));
				}
				shiftbuf(buf, w->t.n.len);
				break;
			case STR: {
				char *esc = "\\\a\b\f\n\r\t\v";
				for (unsigned int i=0; i<*w->t.s.plen; i++) {
					char *pesc = strchr(esc, buf->mem[i]);
					if (pesc) {
						if (printf("\\%c", "\\abfnrtv"[pesc-esc]) < 0)
							die("li: printf: %s", strerror(errno));
					} else if (putchar(buf->mem[i]) == EOF)
						die("li: putchar: %s", strerror(errno));
				}
				shiftbuf(buf, *w->t.s.plen);
				break;
			}}
			if (print && (putchar('\n') == EOF))
				die("li: putchar: %s", strerror(errno));
		}
		wnodes = w->nexts;
		nwnodes = w->nnexts;
		iwnodes = 0;
nextnode:;
	}
	if (!nwnodes || fillbuf(buf, 1))
		return;
	fputs("li: expected ", stderr);
	for (iwnodes=0; iwnodes<nwnodes; iwnodes++) {
		struct wordnode *w = wnodes+iwnodes;
		if (iwnodes)
			fputs(" or ", stderr);
		if (*w->name)
			fputs(w->name, stderr);
		fputc('(', stderr);
				for (unsigned int i=0; i<*w->t.s.plen; i++)
		switch (w->type) {
		case NUM:
			fprintf(stderr, "%c%u", w->t.n.format, w->t.n.len);
			if (w->t.n.isconst)
				fprintf(stderr, ":%u", w->t.n.val);
			break;
		case STR:
			fprintf(stderr, "s%u", *w->t.s.plen);
			break;
		case PHRASE:
			fputs(w->t.p->name, stderr);
			break;
		}
		fputc(')', stderr);
		if (*w->pntimes != 1)
			fprintf(stderr, "{%u}", *w->pntimes);
	}
	fputc('\n', stderr);
bytes:
	fprintf(stderr, "%2hhX", buf->mem[0]);
	for (unsigned int i=1; i<buf->len; i++)
		fprintf(stderr, "\t%2hhX", buf->mem[i]);
	int c;
	while ((c = getchar()) != EOF)
		fprintf(stderr, "\t%2hhX", c);
	if (ferror(stdin))
		die("li: getchar: %s", strerror(errno));
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		die("usage: li labels");
	FILE *f = fopen(argv[1], "r");
	if (!f)
		die("li: fopen: %s", strerror(errno));
	struct phrase *phrases = NULL;
	unsigned int nphrases = 0;
	struct wordnode *msgs = NULL;
	unsigned int nmsgs = 0;
	int line=1, col=0;
	int c = getlabelc(f, &line, &col);
	if (c == EOF)
		die("li: empty labels file");
	while (c != EOF) {
		if ((c != '(') && strchr(SPECIAL, c))
			die("li: expected ( or name at line %u, col %u", line, col);
		unsigned int *pn;
		struct wordnode **pwords;
		char namebuf[MAXNAME+1] = {0};
		if ((c != '(') && ((c=getname(f,namebuf,c,&line,&col)) == ':')) {
			if (!(phrases=realloc(phrases, sizeof(struct phrase)*++nphrases)))
				die("li: realloc %u phrases: %s", nphrases, strerror(errno));
			pwords=&phrases[nphrases-1].firsts, pn=&phrases[nphrases-1].nfirsts;
			*pwords=NULL, *pn=0;
			strcpy(phrases[nphrases-1].name, namebuf);
			*namebuf = '\0';
			c = getlabelc(f, &line, &col);
		} else
			pwords=&msgs, pn=&nmsgs;
		struct parsenode pahead = {.pwords=pwords, .pn=pn};
		struct parsenode *panode = &pahead;
		int indent = 0;
		while (*namebuf || (c != EOF)) {
			if (c == '\n') {
				if (*namebuf)
					die("li: name expected type at end of line %u", line-1);
				while ((c=getlabelc(f,&line,&col)) == '\t');
				int newindent = (pn == &nmsgs) + col - 1;
				if (newindent < 1)
					break;
				for (int i=0; i<indent-newindent+1; i++) {
					struct parsenode *tmp = panode->prev;
					free(panode);
					panode = tmp;
				}
				pwords=panode->pwords, pn=panode->pn;
				if (newindent > indent+1)
					die("li: line %u too indented", line-(c=='\n'));
				indent = newindent;
			}
			if (!(*pwords=realloc(*pwords, sizeof(struct wordnode)*++*pn)))
				die("li: realloc %u words: %s", *pn, strerror(errno));
			struct parsenode *newpanode = malloc(sizeof(struct parsenode));
			if (!newpanode)
				die("li: malloc new parse node: %s", strerror(errno));
			newpanode->prev = panode;
			struct wordnode *w = (panode=newpanode)->w = (*pwords)+*pn-1;
			panode->pwords=pwords, panode->pn=pn;
			*w->name = '\0';
			if (*namebuf)
				strcpy(w->name, namebuf);
			else if (!strchr(SPECIAL, c))
				c = getname(f, w->name, c, &line, &col);
			if (c != '(')
				die("li: expected ( at line %u, col %u", line, col);
			c = getname(f, namebuf, 0, &line, &col);
			if (strchr("bu", *namebuf) && isdigits(namebuf+1)) {
				w->type = NUM;
				w->t.n.format = *namebuf;
				if (namebuf[1] && (namebuf[2] || strchr("09", namebuf[1])))
					die("li: bad length before line %u, col %u", line, col);
				if (namebuf[1])
					w->t.n.len = namebuf[1] - '0';
				else
					w->t.n.len = 1;
				if ((w->t.n.isconst=(c == ':'))) {
					if (w->t.n.format == 'b')
						die("li: binary constant not supported (line %u, col %u)", line, col);
					c = getname(f, namebuf, 0, &line, &col);
					if (!isdigits(namebuf))
						die("li: expected digits before line %u, col %u", line, col);
					if (errno=0, w->t.n.val=strtoul(namebuf+1, NULL, 10), errno)
						die("li: strtoul (line %u, col %u): %s", line, col, strerror(errno));
				}
			} else if ((*namebuf == 's') && isdigits(namebuf+1)) {
				w->type = STR;
				if (namebuf[1]) {
					if (errno=0, w->t.s.lenstorage=strtoul(namebuf+1, NULL, 0), errno)
						die("li: strtoul (line %u, col %u): %s", line, col, strerror(errno));
					w->t.s.plen = &w->t.s.lenstorage;
				} else if (c == '$')
					w->t.s.plen = getpnum(f, &c, &line, &col, panode, &pahead);
				else
					w->t.s.lenstorage=1, w->t.s.plen=&w->t.s.lenstorage;
			} else {
				w->type = PHRASE;
				w->t.p = NULL;
				for (int i=0; i<nphrases; i++)
					if (!strcmp(namebuf, phrases[i].name)) {
						w->t.p = phrases+i;
						break;
					}
				if (!w->t.p)
					die("li: name before line %u, col %u not found", line, col);
			}
			if (c != ')')
				die("li: expected ) at line %u, col %u", line, col);
			if ((c=getlabelc(f,&line,&col)) == '{') {
				if ((c=getlabelc(f,&line,&col)) == '$')
					w->pntimes = getpnum(f, &c, &line, &col, panode, &pahead);
				else if ((c == EOF) || strchr(SPECIAL, c))
					die("li: expected number at line %u, col %u", line, col);
				else {
					c = getname(f, namebuf, 0, &line, &col);
					if (!isdigits(namebuf))
						die("li: expected digits before line %u, col %u", line, col);
					if (errno=0, w->ntimesstorage=strtoul(namebuf, NULL, 10), errno)
						die("li: strtoul before line %u, col %u: %s", line, col, strerror(errno));
					w->pntimes = &w->ntimesstorage;
				}
				if (c != '}')
					die("li: expected } at line %u, col %u", line, col);
				c = getlabelc(f, &line, &col);
			} else
				w->ntimesstorage=1, w->pntimes=&w->ntimesstorage;
			*namebuf = '\0';
		}
		while (panode != &pahead) {
			struct parsenode *tmp = panode->prev;
			free(panode);
			panode = tmp;
		}
	}
	if (fclose(f))
		die("li: fclose: %s", strerror(errno));
	struct buffer buf = {0};
	struct buffer prefix = {.mem=malloc(1), .len=0, .cap=1};
	if (!prefix.mem)
		die("li: malloc prefix 1: %s", strerror(errno));
	while (!fillbuf(&buf, 1))
		labelmsg(&buf, msgs, nmsgs, &prefix);
}
