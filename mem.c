#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>


struct entry {
	char *a, *b;
	int day, score;
};


volatile sig_atomic_t sigd = 0;
int ttyfd;
struct termios ttyold, ttyraw;


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

void
sigint(int sig)
{
	sigd = 1;
}

void
diesafe(char *msg, int err)
{
	int pten = 1, tmp;
	write(2, msg, strlen(msg));
	if (err < 0)
		write(2, "-", 1);
	while ((err/pten > 9) || (err/pten < -9))
		pten*=10;
	for (; pten; pten/=10)
		tmp=err/pten%10*(1-2*(err<0))+'0', write(2, &tmp, 1);
	write(2, "\n", 1);
	_exit(EXIT_FAILURE);
}

void
settty(int fd, struct termios *info)
{
	int res;
	do res = tcsetattr(fd, TCSAFLUSH, info); while ((res==-1) && (errno==EINTR));
	if (res == -1)
		diesafe("mem: tcsetattr: errno ", errno);
}

void
sigtstp(int sig)
{
	settty(ttyfd, &ttyold);
	if (signal(SIGTSTP, SIG_DFL) == SIG_ERR)
		diesafe("mem: signal SIGTSTP SIG_DFL: errno ", errno);
	raise(SIGTSTP);
}

void
inittstptty(int sig)
{
	if (signal(SIGTSTP, sigtstp) == SIG_ERR)
		diesafe("mem: signal SIGTSTP sigtstp: errno ", errno);
	settty(ttyfd, &ttyraw);
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		die("usage: mem file");

	// Read entries
	FILE *f = fopen(argv[1], "r");
	if (!f)
		die("mem: fopen r: %s", strerror(errno));
	unsigned int maxentries = 1024;
	struct entry *entries = malloc(maxentries * sizeof(struct entry));
	if (!entries)
		die("mem: malloc: %s", strerror(errno));
	unsigned int nentries = 0;
	int res;
	struct entry *e;
	while (errno=0, e=entries+nentries, (res=fscanf(f, "%m[^\t] %m[^\t] %u %u ",
			&e->a, &e->b, &e->day, &e->score)) == 4)
		if ((++nentries == maxentries) && !(entries=realloc(entries,
				sizeof(struct entry) * (maxentries*=2))))
			die("mem: realloc %u entries: %s", maxentries, strerror(errno));
	if (ferror(f))
		die("mem: fscanf read error at entry %u: %s", nentries+1, strerror(errno));
	if (errno == ENOMEM)
		die("mem: fscanf malloc: not enough memory for entry %u", nentries+1);
	if (res != EOF)
		die("mem: entry %u is bad", nentries+1);
	if (fclose(f))
		die("mem: fclose r: %s", strerror(errno));
	if (!nentries)
		return 0;

	// Get today's naive 'daystamp'
	time_t t = time(NULL);
	if (t == -1)
		die("mem: time: %s", strerror(errno));
	struct tm *tm = localtime(&t);
	if (!tm)
		die("mem: localtime: %s", strerror(errno));
	int today = (1900+tm->tm_year)*365 + tm->tm_yday + 1;

	// Init unbuffered tty and signal handlers
	if ((ttyfd=open("/dev/tty", O_RDONLY)) == -1)
		die("mem: open /dev/tty: %s", strerror(errno));
	if (tcgetattr(ttyfd, &ttyold))
		die("mem: tcgetattr: %s", strerror(errno));
	ttyraw = ttyold;
	ttyraw.c_lflag &= ~(ECHO | ICANON);
	ttyraw.c_cc[VMIN] = 1;
	ttyraw.c_cc[VTIME] = 0;
	struct sigaction sa = { .sa_flags = SA_RESETHAND, .sa_handler = sigint };
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGINT, &sa, NULL))
		die("mem: sigaction SIGINT: %s", strerror(errno));
	sa = (struct sigaction){ .sa_handler = inittstptty };
	if (sigaction(SIGCONT, &sa, NULL))
		die("mem: sigaction SIGCONT: %s", strerror(errno));
	inittstptty(0);

	// Practice weakest entry until none <= 0.5
	srand(t);
	for (;;) {

		// Find weakest entry
		struct entry *weakest = entries;
		double pmin = 1.0;
		for (unsigned int i=0; i<nentries; i++) {
			double praw = pow(2.0, -(today-entries[i].day)/pow(2.0,entries[i].score));
			double p = praw - 0.2*rand()/RAND_MAX;  // shuffle a little
			if ((praw <= 0.5) && (p < pmin))
				pmin=p, weakest=entries+i;
		}
		if (pmin > 0.5)
			break;

		// Practice entry
		int r = 1;
		char c;
		for (int i=0; i<2 && !sigd && r; i++) {
			if (puts(i ? weakest->b : weakest->a) == EOF)
				die("mem: puts: %s", strerror(errno));
			do; while (!sigd && ((((r=read(ttyfd, &c, 1)) > 0) && !strchr(" \n", c))
					|| ((r == -1) && (errno == EINTR))));
			if (!sigd && (r == -1))
				die("mem: read: %s", strerror(errno));
		}
		if (sigd || !r)
			break;
		weakest->score += (c == ' ') ? -1 : 1;
		if (weakest->score < 1)
			weakest->score = 1;
		weakest->day = today;
	}

	// Save
	if (!(f=fopen(argv[1], "w")))
		die("mem: fopen w: %s", strerror(errno));
	for (int i=0; i<nentries && (e=entries+i); i++)
		if (fprintf(f, "%s\t%s\t%u\t%u\n", e->a, e->b, e->day, e->score) < 0)
			die("mem: fprintf: %s", strerror(errno));

	// Reset tty
	settty(ttyfd, &ttyold);
}
