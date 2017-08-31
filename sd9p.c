#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


const int THREADC = 16;
const int MAXFIDS = 64;
const int MINLEN = 256;  // >= 237 (largest rwalk message)
const uint8_t TVERSION = 100;
const uint8_t TAUTH = 102;
const uint8_t TATTACH = 104;
const uint8_t TFLUSH = 108;
const uint8_t TWALK = 110;
const uint8_t TOPEN = 112;
const uint8_t TCREATE = 114;
const uint8_t TREAD = 116;
const uint8_t TWRITE = 118;
const uint8_t TCLUNK = 120;
const uint8_t TREMOVE = 122;
const uint8_t TSTAT = 124;
const uint8_t TWSTAT = 126;
const uint16_t NOTAG = ~0;
const uint32_t NOFID = ~0;
const uint8_t QTDIR = 1<<7;
const uint8_t QTAPPEND = 1<<6;
const uint8_t QTAUTH = 1<<3;


enum alert { NONE, FLUSH, ABORT };
struct worker {
	pthread_t id;
	enum alert alert;
	pthread_mutex_t alertlock;
	uint16_t tag;
};
struct anode {
	char *name;
	struct anode *parent;
	int fd;
	unsigned int refs;
	pthread_rwlock_t lock;
};
struct auth {
	pid_t pidstatus;  // 0/1 is exit status, otherwise pid
	int r, w;
	char *uname, *aname;
};
struct file {
	int fd;
	struct anode *pdir, *dir;
	char *name;
	char *dirent;
};
union info {
	struct auth auth;
	struct file f;
};
struct finfo {
	uint32_t fid;
	uint64_t qid;
	uint64_t offset;
	union info i;
};
struct qid {
	dev_t st_dev;
	ino_t st_ino;
	uint8_t type;
	uint32_t version;
};


int ARGC;
char **ARGV;
uint32_t MSGLEN = 0;
char *MSGBUF;
sem_t MSGNEW, MSGGOT;
pthread_mutex_t WLOCK, STRERR;
pthread_rwlock_t TAGLOCK;  // 'reader' = worker writing, 'writer' = main reading
struct finfo FIDS[MAXFIDS];
pthread_rwlock_t FIDLOCK;
struct qid *QIDS = NULL;
uint64_t QIDC = 0;
pthread_rwlock_t QIDLOCK;


void
sigusr1(int sig)
{  // This function intentionally left blank
}

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
uint2le(char *buf, int len, uint64_t d)
{
	for (int i=0; i<len; i++)
		buf[i] = d >> 8*i;
}

char*
strerr(int errnum)
{
	int res = pthread_mutex_lock(&STRERR);  // assuming about to die
	if (res)
		die("sd9p: pthread_mutex_lock STRERR: errnum %d after %d", res, errnum);
	return strerror(errnum);
}

void
lock(pthread_mutex_t *lock, char *s)
{
	int res = pthread_mutex_lock(lock);
	if (res)
		die("sd9p: pthread_mutex_lock %s: %s", s, strerr(res));
}

void
unlock(pthread_mutex_t *lock, char *s)
{
	int res = pthread_mutex_unlock(lock);
	if (res)
		die("sd9p: pthread_mutex_unlock %s: %s", s, strerr(res));
}

void
say(char type, char *buf, size_t bodylen)
{
	uint2le(buf, 4, bodylen+7);
	buf[4] = type;
	lock(&WLOCK, "write lock");
	size_t w = fwrite(buf, 1, bodylen+7, stdout);
	if (ferror(stdout))
		die("sd9p: fwrite: %s", strerr(errno));
	if (w < bodylen+7)
		exit(EXIT_SUCCESS);
	unlock(&WLOCK, "write lock");
}

uint64_t
le2uint(char *buf, int len)
{
	uint64_t d = 0;
	for (int i=0; i<len; i++)
		d += buf[i] << 8*i;
	return d;
}

void
sayerr(char *buf, char *errfmt, ...)
{
	va_list argp;
	va_start(argp, errfmt);
	vsnprintf(buf+9, MINLEN-9, errfmt, argp);
	va_end(argp);
	size_t len = strlen(buf+9);
	uint2le(buf+7, 2, len);
	say(107, buf, len+2);
}

int
parsemsg(char *msg, char *fmt, ...)
{
	uint32_t len = le2uint(msg, 4);
	char *ss[strlen(fmt)];  // in case of err, save pointers so they can be freed
	memset(ss, 0, sizeof(ss));
	uint32_t i=7;
	va_list ap;
	va_start(ap, fmt);
	int err=0;
	for (char *cp=fmt; *cp; cp++) {
		if ((isdigit(*cp) && (len < i+*cp-'0')) ||
				((*cp == 's') && ((len < i+2) || (len < i+2+le2uint(msg+i, 2))))) {
			err=1, sayerr(msg, "short message");
			break;
		}
		switch (*cp) {
		case '1': {
			*va_arg(ap, uint8_t*) = le2uint(msg+i, 1);
			i += 1;
			break;
		} case '2': {
			*va_arg(ap, uint16_t*) = le2uint(msg+i, 2);
			i += 2;
			break;
		} case '4': {
			*va_arg(ap, uint32_t*) = le2uint(msg+i, 4);
			i += 4;
			break;
		} case '8': {
			*va_arg(ap, uint64_t*) = le2uint(msg+i, 8);
			i += 8;
			break;
		} case 's': {
			uint16_t len = le2uint(msg+i, 2);
			char **s = va_arg(ap, char**);
			if (!((ss[cp-fmt] = *s = malloc(len+1))))
				die("sd9p: malloc %d: %s", len+1, strerr(errno));
			*stpncpy(*s, msg+i+2, len) = '\0';
			i += 2+len;
			break;
		}
		}
	}
	va_end(ap);
	if (!err && (i != len))
		err=1, sayerr(msg, "long message");
	if (err) {
		for (size_t i=0; i<sizeof(ss)/sizeof(char*); i++)
			free(ss[i]);
		return 1;
	}
	return 0;
}

void
cleananame(char **aname)
{
	*aname = realloc(*aname, strlen(*aname)+3);
	char *s = *aname;
	uint16_t len = strlen(s);

	// Add leading and trailing slash
	memmove(s+1, s, (len++)+1);
	s[0] = '/';
	s[len++] = '/';
	s[len] = '\0';

	// Remove double slashes and .'s
	uint16_t i = 0;
	while (i < len-1)
		if ((s[i] == '/') && (s[i+1] == '/'))
			memmove(s+i, s+i+1, len-i), len--;
		else if ((s[i] == '/') && (s[i+1] == '.') && (s[i+2] == '/'))
			memmove(s+i, s+i+2, len-i-1), len-=2;
		else
			i++;

	// Remove ..'s
	i = len-1;
	while (i >= 3)
		if ((s[i]=='/') && (s[i-1]=='.') && (s[i-2]=='.') && (s[i-3]=='/')) {
			uint16_t end = i;
			i -= 3;
			do; while (i && (s[--i] != '/'));
			memmove(s+i, s+end, len-end+1);
			len -= end-i;
		} else
			i--;

	*aname = realloc(*aname, len+1);
}

void
wlock(pthread_rwlock_t *lock, char *s)
{
	int res = pthread_rwlock_wrlock(lock);
	if (res)
		die("sd9p: pthread_rwlock_wrlock %s: %s", s, strerr(res));
}

int
getfidi(uint32_t fid)
{
	int unusedfidi = MAXFIDS;
	for (int fidi=0; fidi<MAXFIDS; fidi++)
		if (FIDS[fidi].fid == fid)
			return fidi;
		else if ((unusedfidi == MAXFIDS) && (FIDS[fidi].fid == NOFID))
			unusedfidi = fidi;
	return unusedfidi;
}

void
rwunlock(pthread_rwlock_t *lock, char *s)
{
	int res = pthread_rwlock_unlock(lock);
	if (res)
		die("sd9p: pthread_rwlock_unlock %s: %s", s, strerr(res));
}

uint64_t
matchqid(struct stat *buf)
{
	uint64_t qid;
	for (qid=0; qid<QIDC; qid++)
		if ((QIDS[qid].st_dev == buf->st_dev) && (QIDS[qid].st_ino == buf->st_ino))
			break;
	return qid;
}

void
rlock(pthread_rwlock_t *lock, char *s)
{
	int res = pthread_rwlock_rdlock(lock);
	if (res)
		die("sd9p: pthread_rwlock_rdlock %s: %s", s, strerr(res));
}

uint64_t
getqid(struct stat *buf, uint8_t type)
{
	// rlock QIDLOCK???
	uint64_t qid = matchqid(buf);
	if (qid == QIDC) {
		rwunlock(&QIDLOCK, "QIDLOCK getqid start");
		wlock(&QIDLOCK, "QIDLOCK getqid no match");
		if ((qid = matchqid(buf)) == QIDC) {
			if (!(QIDC & (QIDC-1))) { // power of two (at capacity)
				size_t capacity = (QIDC ? (QIDC<<1) : 1) * sizeof(struct qid);
				if (!((QIDS = realloc(QIDS, capacity))))
					die("sd9p: realloc qids (%zu): %s", capacity, strerr(errno));
			}
			QIDC++;
			QIDS[qid].st_dev = buf->st_dev;
			QIDS[qid].st_ino = buf->st_ino;
			QIDS[qid].type = type;
			if (S_ISDIR(buf->st_mode))
				QIDS[qid].type |= QTDIR;
			if (S_ISFIFO(buf->st_mode) || S_ISSOCK(buf->st_mode))
				QIDS[qid].type |= QTAPPEND;
			QIDS[qid].version = 0;
		}
	}
	rwunlock(&QIDLOCK, "QIDLOCK getqid end");
	return qid;
}

void
wqid(char *buf, uint64_t qid)
{
	rlock(&QIDLOCK, "QIDLOCK wqid");
	uint2le(buf, 1, QIDS[qid].type);
	uint2le(buf+1, 4, QIDS[qid].version);
	uint2le(buf+1+4, 8, qid);
	rwunlock(&QIDLOCK, "QIDLOCK wqid");
}

void
saystrerr(char *buf, char *s, int errnum)
{
	char errs[128];
	if (strerror_r(errnum, errs, sizeof(errs)))
		snprintf(errs, sizeof(errs), "errnum %d (long error message)", errnum);
	sayerr(buf, "%s: %s", s, errs);
}

void*
worker(void *info_)
{
	struct worker *info = info_;
	uint32_t len = 0;
	char *msgbuf = NULL;
	while (1) {
		int res;
		do; while (((res=sem_wait(&MSGNEW))) && (errno==EINTR));
		if (res)
			die("sd9p: sem_wait MSGNEW: %s", strerr(errno));
		if ((len != MSGLEN) && !((free(msgbuf),msgbuf=malloc((len=MSGLEN)))))
			die("sd9p: malloc msgbuf: %s", strerr(errno));
		char *tmp = MSGBUF;
		MSGBUF = msgbuf, msgbuf = tmp;
		info->alert = NONE;
		info->tag = le2uint(msgbuf+5, 2);
		lock(&info->alertlock, "alertlock start");
		if (sem_post(&MSGGOT))
			die("sd9p: sem_post MSGGOT: %s", strerr(errno));

		if (le2uint(msgbuf+5, 2) == NOTAG) {
			sayerr(msgbuf, "NOTAG can only be used for version messages");
			goto END;
		}

		switch (msgbuf[4]) {
		case TAUTH: {
			if (ARGC < 3) {
				sayerr(msgbuf, "no authorization needed");
				goto END;
			}
			uint32_t afid;
			char *uname, *aname;
			if (parsemsg(msgbuf, "4ss", &afid, &uname, &aname))
				goto END;
			cleananame(&aname);
			if (afid == NOFID) {
				sayerr(msgbuf, "can't use NOFID as a normal fid");
				goto AUTHFREE;
			}
			wlock(&FIDLOCK, "FIDLOCK auth");
			int fidi = getfidi(afid);
			if (fidi == MAXFIDS)
				sayerr(msgbuf, "maximum fids reached (%d)", MAXFIDS);
			else if (FIDS[fidi].fid != NOFID)
				sayerr(msgbuf, "fid %d already in use", afid);
			else {
				FIDS[fidi].fid = afid;
				FIDS[fidi].i.auth.uname = uname;
				FIDS[fidi].i.auth.aname = aname;
				uname = aname = NULL;
				int fds[4];
				if (pipe(fds) || pipe(fds+2))
					die("sd9p: pipe: %s", strerr(errno));
				for (int i=0; i<=3; i+=3) {
					int flags = fcntl(fds[i], F_GETFD);
					if (flags == -1)
						die("sd9p: fcntl F_GETFD: %s", strerr(errno));
					if (fcntl(fds[i], F_SETFD, flags|O_CLOEXEC) == -1)
						die("sd9p: fcntl F_SETFD: %s", strerr(errno));
				}
				if ((FIDS[fidi].i.auth.pidstatus = fork()) == -1)
					die("sd9p: fork: %s", strerr(errno));
				if (!FIDS[fidi].i.auth.pidstatus) {
					if ((dup2(fds[1],1)==-1) || (dup2(fds[2],0)==-1))
						die("sd9p: dup2: %s", strerror(errno));
					if (execvp(ARGV[2], ARGV+2) == -1)
						die("sd9p: execvp: %s", strerror(errno));
				}
				if (close(fds[1]) || close(fds[2]))
					die("sd9p: close pipe: %s", strerr(errno));
				FIDS[fidi].i.auth.r = fds[0], FIDS[fidi].i.auth.w = fds[3];
				struct stat buf;
				if (fstat(FIDS[fidi].i.auth.r, &buf))
					die("sd9p: fstat auth r: %s", strerr(errno));
				FIDS[fidi].qid = getqid(&buf, QTAUTH);
				wqid(msgbuf+7, FIDS[fidi].qid);
				say(TAUTH+1, msgbuf, 13);
			}
			rwunlock(&FIDLOCK, "FIDLOCK auth");
AUTHFREE:
			free(uname), free(aname);
			break;

		} case TATTACH: {
			uint32_t fid, afid;
			char *uname, *aname;
			int fd = -1;
			if (parsemsg(msgbuf, "44ss", &fid, &afid, &uname, &aname))
				goto ATTACHFREE;
			cleananame(&aname);
			if (ARGC > 2) {
				if (afid == NOFID) {
					sayerr(msgbuf, "authentication required");
					goto ATTACHFREE;
				}
				int err=0;
				wlock(&FIDLOCK, "FIDLOCK attach afid check");
				int fidi = getfidi(afid);
				rlock(&QIDLOCK, "QIDLOCK attach afid check");
				if ((fidi == MAXFIDS) || (FIDS[fidi].fid == NOFID))
					err=1, sayerr(msgbuf, "afid not found");
				else if (!(QIDS[FIDS[fidi].qid].type & QTAUTH))
					err=1, sayerr(msgbuf, "afid not open for authentication");
				else if (strcmp(uname, FIDS[fidi].i.auth.uname))
					err=1, sayerr(msgbuf, "uname doesn't match afid's uname");
				else if (strcmp(aname, FIDS[fidi].i.auth.aname))
					err=1, sayerr(msgbuf, "aname doesn't match afid's aname");
				else if (FIDS[fidi].i.auth.pidstatus) {
					if (FIDS[fidi].i.auth.pidstatus > 1) {
						pid_t pid = FIDS[fidi].i.auth.pidstatus;
						int statloc;
						if (((res = waitpid(pid, &statloc, WNOHANG))) == -1)
							die("sd9p: waitpid %ld attach: %s", pid, strerr(errno));
						if (res && WIFEXITED(statloc))
							FIDS[fidi].i.auth.pidstatus = !!WEXITSTATUS(statloc);
						else if (res && WIFSIGNALED(statloc))
							FIDS[fidi].i.auth.pidstatus = 1;
					}
					if (FIDS[fidi].i.auth.pidstatus > 1)
						err=1, sayerr(msgbuf, "authentication incomplete");
					else if (FIDS[fidi].i.auth.pidstatus)
						err=1, sayerr(msgbuf, "authentication unsuccessful");
				}
				rwunlock(&QIDLOCK, "QIDLOCK attach afid check");
				rwunlock(&FIDLOCK, "FIDLOCK attach afid check");
				if (err)
					goto ATTACHFREE;
			}
			if (!((aname = realloc(aname, strlen(aname)+strlen(ARGV[1])+1))))
				die("sd9p: realloc aname: %s", strerr(errno));
			memmove(aname+strlen(ARGV[1]), aname, strlen(aname)+1);
			memcpy(aname, ARGV[1], strlen(ARGV[1]));
			unlock(&info->alertlock, "alertlock attach open");
			fd = open(aname, O_DIRECTORY|O_CLOEXEC|O_SEARCH);
			lock(&info->alertlock, "alertlock attach open");
			if (info->alert != NONE)
				goto ATTACHFREE;
			if (fd == -1) {
				if (errno == EACCES)
					sayerr(msgbuf, "authenticated, but permission denied");
				else if ((errno == ENOENT) || (errno == ENOTDIR))
					sayerr(msgbuf, "aname directory does not exist");
				else
					saystrerr(msgbuf, "open", errno);
				goto ATTACHFREE;
			}
			unlock(&info->alertlock, "alertlock attach fstat");
			struct stat buf;
			res = fstat(fd, &buf);
			lock(&info->alertlock, "alertlock attach fstat");
			if (info->alert != NONE)
				goto ATTACHFREE;
			if (res == -1) {
				saystrerr(msgbuf, "stat", errno);
				goto ATTACHFREE;
			}
			wlock(&FIDLOCK, "FIDLOCK attach entry");
			int fidi = getfidi(fid);
			if (fidi == MAXFIDS)
				sayerr(msgbuf, "maximum fids reached (%d)", MAXFIDS);
			else if (FIDS[fidi].fid != NOFID)
				sayerr(msgbuf, "fid %d already in use", fid);
			else {
				FIDS[fidi].fid = fid;
				FIDS[fidi].qid = getqid(&buf, 0);
				FIDS[fidi].offset = 0;
				FIDS[fidi].i.f.dir = malloc(sizeof(struct anode));
				FIDS[fidi].i.f.dir->name = "/";
				FIDS[fidi].i.f.dir->parent = FIDS[fidi].i.f.dir;
				FIDS[fidi].i.f.dir->fd = fd;
				FIDS[fidi].i.f.dir->refs = 0;
				if ((res=pthread_rwlock_init(&FIDS[fidi].i.f.dir->lock)))
					die("sd9p: pthread_rwlock_init attach dir: %s", strerr(res));
				FIDS[fidi].i.f.name = "/";
				FIDS[fidi].i.f.dirent = NULL;
				wqid(msgbuf+7, FIDS[fidi].qid);
				say(TATTACH+1, msgbuf, 13);
			}
			rwunlock(&FIDLOCK, "FIDLOCK attach entry");
ATTACHFREE:
			free(aname), free(uname);
			if (fd != -1)
				close(fd);
			break;

		} default:
			sayerr(msgbuf, "unknown message type %d", msgbuf[4]);
			break;
		}

END:
		rlock(&TAGLOCK, "TAGLOCK end");
		if (info->alert == FLUSH) {  // alertlock should be held
			uint2le(msgbuf+5, 2, info->tag);
			say(TFLUSH+1, msgbuf, 0);
			info->alert = NONE;
		}
		info->tag = NOTAG;
		rwunlock(&TAGLOCK, "TAGLOCK end");
		unlock(&info->alertlock, "alert end");
	}
}

void
get(char *dest, size_t len, char *buf)
{
	size_t r = fread(dest, 1, len, stdin);
	if (ferror(stdin)) {
		int errnum = errno;
		sayerr(buf, "error reading input");
		die("sd9p: fread: %s", strerr(errnum));
	}
	if (r < len) {
		if (r || (buf != dest))  // Ended in middle of a message
			sayerr(buf, "short read - expected at least %d, got %d", len, r);
		exit(EXIT_SUCCESS);
	}
}

void
waitpidordie(pid_t pid, int *statloc)
{
	int res;
	do res=waitpid(pid, statloc, 0); while ((res == -1) && (errno == EINTR));
	if (res == -1)
		die("sd9p: waitpid %ld: %s", pid, strerr(errno));
}

int
main(int argc, char **argv)
{
	setbuf(stdout, NULL);
	if (signal(SIGUSR1, sigusr1) == SIG_ERR)
		die("sd9p: signal: %s", strerror(errno));
	if (argc < 2)
		die("usage: sd9p dir [authcmd [arg ...]]");
	ARGC = argc, ARGV = argv;

	// Initialize global variables
	MSGBUF = malloc(MINLEN);
	if (!MSGBUF)
		die("sd9p: malloc first %d (?!) bytes: %s", MINLEN, strerror(errno));
	if (sem_init(&MSGNEW, 0, 0))
		die("sd9p: sem_init MSGNEW: %s", strerror(errno));
	if (sem_init(&MSGGOT, 0, 0))
		die("sd9p: sem_init MSGGOT: %s", strerror(errno));
	int res = pthread_mutex_init(&WLOCK, NULL);
	if (res)
		die("sd9p: pthread_mutex_init WLOCK: %s", strerror(res));
	if ((res=pthread_mutex_init(&STRERR, NULL)))
		die("sd9p: pthread_mutex_init STRERR: %s", strerror(res));
	if ((res=pthread_rwlock_init(&TAGLOCK, NULL)))
		die("sd9p: pthread_rwlock_init TAGLOCK: %s", strerror(res));
	if ((res=pthread_rwlock_init(&FIDLOCK, NULL)))
		die("sd9p: pthread_rwlock_init FIDLOCK: %s", strerror(res));
	if ((res=pthread_rwlock_init(&QIDLOCK, NULL)))
		die("sd9p: pthread_rwlock_init QIDLOCK: %s", strerror(res));
	for (int i=0; i<MAXFIDS; i++)
		FIDS[i].fid = NOFID;

	// Create worker threads
	struct worker threads[THREADC];
	for (int i=0; i<THREADC; i++) {
		threads[i].alert = NONE;
		if ((res=pthread_mutex_init(&threads[i].alertlock, NULL)))
			die("sd9p: pthread_mutex_init thread %d: %s", i, strerr(res));
		threads[i].tag = NOTAG;
		if ((res=pthread_create(&threads[i].id, NULL, worker, &threads[i])))
			die("sd9p: pthread_create %d: %s", i, strerr(res));
	}

	while (!feof(stdin)) {
		// Read in message
		MSGBUF[5]=MSGBUF[6]=0xFF, get(MSGBUF, 4, MSGBUF);
		uint32_t msglen = le2uint(MSGBUF, 4);
		if (msglen < 7) {
			sayerr(MSGBUF, "stub message (shorter than 7 bytes? wtf?)");
			continue;
		}
		if (!MSGLEN && (msglen > MINLEN)) {  // First message & longer than default
			if (!(MSGBUF = realloc(MSGBUF, msglen)))
				die("sd9p: realloc: %s", strerr(errno));
			get(MSGBUF+4, msglen-4, MSGBUF);
		} else if (MSGLEN && (msglen > MSGLEN)) {  // Message too long
			get(MSGBUF+4, 3, MSGBUF);
			sayerr(MSGBUF, "message length %u > %u", msglen, MSGLEN);
			do get(MSGBUF+7,MSGLEN-7,MSGBUF); while ((msglen-=(MSGLEN-7))-7 > MSGLEN);
			get(MSGBUF+7, msglen-7, MSGBUF);
			continue;
		} else  // Message fits
			get(MSGBUF+4, msglen-4, MSGBUF);

		// First message must be version message
		if (!MSGLEN && (MSGBUF[4] != TVERSION)) {
			sayerr(MSGBUF, "version message required");
			continue;
		}

		// Handle version messages
		if (MSGBUF[4] == TVERSION) {
			uint32_t newmsglen;
			char *version;
			if (parsemsg(MSGBUF, "4s", &newmsglen, &version))
				continue;
			res = strncmp(version,"9P2000",6) || (version[6] && (version[6] != '.'));
			free(version);
			if (res) {
				uint2le(MSGBUF+7+4, 2, 7);
				strncpy(MSGBUF+7+6, "unknown", 7);
				say(TVERSION+1, MSGBUF, 4+2+7);
				continue;
			}
			if (newmsglen < MINLEN) {
				sayerr(MSGBUF, "message size too small");
				continue;
			}
			if (!((MSGBUF = realloc(MSGBUF, MSGLEN=newmsglen))))
				die("sd9p: realloc: %s", strerr(errno));
			for (int i=0; i<THREADC; i++) {
				lock(&threads[i].alertlock, "alert abort");
				threads[i].alert = ABORT;
				unlock(&threads[i].alertlock, "alert abort");
				if ((res=pthread_kill(threads[i].id, SIGUSR1)))
					die("sd9p: pthread_kill %d: %s", i, strerr(res));
			}
			wlock(&FIDLOCK, "FIDLOCK reset");
			rlock(&QIDLOCK, "QIDLOCK fid reset");
			for (int i=0; i<MAXFIDS; i++) {
				if (FIDS[i].fid == NOFID)
					continue;
				if (QIDS[FIDS[i].qid].type & QTAUTH) {
					free(FIDS[i].i.auth.uname), free(FIDS[i].i.auth.aname);
					if (close(FIDS[i].i.auth.w) || close(FIDS[i].i.auth.r))
						die("sd9p: close auth fds: %s", strerr(errno));
					if (FIDS[i].i.auth.pidstatus > 1) {
						pid_t pid = FIDS[i].i.auth.pidstatus;
						if (kill(pid, SIGTERM))
							die("sd9p: kill %ld SIGTERM: %s", pid, strerr(errno));
						int statloc;
						waitpidordie(pid, &statloc);
						if (!WIFEXITED(statloc) && !WIFSIGNALED(statloc)) {
							if (kill(pid, SIGKILL))
								die("sd9p: kill %ld SIGKILL: %s", pid, strerr(errno));
							waitpidordie(pid, &statloc);
						}
					}
				} else {
					if (QIDS[FIDS[i].qid].type & QTDIR)
						free(FIDS[i].i.f.dirent);
					else if (fd != -1)
						close(FIDS[i].i.f.fd);
					lock(&FIDS[i].i.f.dir->lock, "anode version");
					while (!--FIDS[i].i.f.dir->refs) {
						close(FIDS[i].i.f.dir->fd);
						unlock(&FIDS[i].i.f.dir->lock, "anode version");
						if ((res=pthread_mutex_destroy(&FIDS[i].i.f.dir->lock)))
							die("sd9p: pthread_mutex_destroy anode version: %s", strerr(res));
						if (FIDS[i].i.f.dir->name[0] == '/') {
							free(FIDS[i].i.f.dir);
							break;
						}
						free(FIDS[i].i.f.dir->name);
						struct anode *pdir = FIDS[i].i.f.dir->parent;
						free(FIDS[i].i.f.dir);
						FIDS[i].i.f.dir = pdir;
						lock(&FIDS[i].i.f.dir->lock, "anode version loop");
					}
				}
				FIDS[i].fid = NOFID;
			}
			rwunlock(&QIDLOCK, "QIDLOCK fid reset");
			rwunlock(&FIDLOCK, "FIDLOCK reset");
			uint2le(MSGBUF+7+4, 2, 6);
			say(TVERSION+1, MSGBUF, 4+2+6);

		// Handle flush message
		} else if (MSGBUF[4] == TFLUSH) {
			uint16_t tag;
			if ((msglen < 9) || ((tag=le2uint(MSGBUF+7,2)) == NOTAG)) {
				say(TFLUSH+1, MSGBUF, 0);
				continue;
			}
			int found = 0;
			wlock(&TAGLOCK, "TAGLOCK send flush");
			for (int i=0; i<THREADC; i++)
				if (threads[i].tag == tag) {
					found=1;
					lock(&threads[i].alertlock, "alert flush");
					threads[i].alert = FLUSH;
					threads[i].tag = tag;
					unlock(&threads[i].alertlock, "alert flush");
					if ((res=pthread_kill(threads[i].id, SIGUSR1)))
						die("sd9p: pthread_kill %d: %s", i, strerr(res));
					break;
				}
			if (!found)
				say(TFLUSH+1, MSGBUF, 0);
			rwunlock(&TAGLOCK, "TAGLOCK send flush");

		// Pass on to a worker
		} else {
			if (sem_post(&MSGNEW))
				die("sd9p: sem_post MSGNEW: %s", strerr(errno));
			do; while (((res=sem_wait(&MSGGOT))) && (errno==EINTR));
			if (res)
				die("sd9p: sem_wait MSGGOT: %s", strerr(errno));
		}
	}
}
