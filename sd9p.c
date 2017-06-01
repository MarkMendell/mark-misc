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


const int THREADC = 32;
const int MAXFIDS = 64;
const int MINLEN = 256;
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
struct auth {
	pid_t pidstatus;  // 0/1 is exit status, otherwise pid
	int r, w;
};
union info {
	struct auth auth;
};
struct finfo {
	uint32_t fid;
	uint64_t qid;
	union info info;
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
	if (pthread_mutex_lock(&STRERR))  // assuming about to die
		die("sd9p: pthread_mutex_lock STRERR: errno %d after %d", errno, errnum);
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
		if ((isdigit(*cp) && (len < i+*cp-'0')) || (len < i+2+le2uint(msg+i, 2))) {
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
			size_t len = le2uint(msg+i, 2);
			char **s = va_arg(ap, char**);
			if (!((ss[cp-fmt] = ((*s = malloc(2+len))))))
				die("sd9p: malloc %d: %s", 2+len, strerr(errno));
			memcpy(*s, msg+i, 2+len);
			i += 2+len;
			break;
		}
		}
	}
	va_end(ap);
	if (!err && (i != len))
		err=1, sayerr(msg, "long message");
	if (err) {
		for (size_t i=0; i<sizeof(ss); i++)
			free(ss[i]);
		return 1;
	}
	return 0;
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
	rlock(&QIDLOCK, "QIDLOCK getqid start");
	uint64_t qid = matchqid(buf);
	if (qid == QIDC) {
		rwunlock(&QIDLOCK, "QIDLOCK getqid start");
		wlock(&QIDLOCK, "QIDLOCK getqid no match");
		if ((qid = matchqid(buf)) == QIDC) {
			if (!(QIDC & (QIDC-1)) &&  // power of two (at capacity)
					!((QIDS=realloc(QIDS, QIDC ? (QIDC << 1) : 1))))
				die("sd9p: realloc qids (%d): %s", QIDC?(QIDC<<1):1, strerr(errno));
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
		if (sem_post(&MSGGOT))
			die("sd9p: sem_post MSGGOT: %s", strerr(errno));

		if (le2uint(msgbuf+5, 2) == NOTAG) {
			sayerr(msgbuf, "NOTAG can only be used for version messages");
			continue;
		}

		switch (msgbuf[4]) {
		case TAUTH: {
			if (ARGC < 2) {
				sayerr(msgbuf, "no authorization needed");
				goto END;
			}
			uint32_t afid;
			char *uname, *aname;
			if (parsemsg(msgbuf, "4ss", &afid, &uname, &aname))
				goto END;
			if (afid == NOFID) {
				sayerr(msgbuf, "can't use NOFID as a normal fid");
				goto END;
			}
			wlock(&FIDLOCK, "FIDLOCK auth");
			int fidi = getfidi(afid);
			if (fidi == MAXFIDS)
				sayerr(msgbuf, "maximum fids reached (%d)", MAXFIDS);
			else if (FIDS[fidi].fid != NOFID)
				sayerr(msgbuf, "fid %d already in use", afid);
			else {
				FIDS[fidi].fid = afid;
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
				if ((FIDS[fidi].info.auth.pidstatus = fork()) == -1)
					die("sd9p: fork: %s", strerr(errno));
				if (!FIDS[fidi].info.auth.pidstatus) {
					if ((dup2(fds[1],1)==-1) || (dup2(fds[2],0)==-1))
						die("sd9p: dup2: %s", strerror(errno));
					if (execvp(ARGV[1], ARGV+1) == -1)
						die("sd9p: execvp: %s", strerror(errno));
				}
				if (close(fds[1]) || close(fds[2]))
					die("sd9p: close pipe: %s", strerr(errno));
				FIDS[fidi].info.auth.r = fds[0], FIDS[fidi].info.auth.w = fds[3];
				struct stat buf;
				if (fstat(FIDS[fidi].info.auth.r, &buf))
					die("sd9p: fstat auth r: %s", strerr(errno));
				FIDS[fidi].qid = getqid(&buf, QTAUTH);
				wqid(msgbuf+7, FIDS[fidi].qid);
				say(TAUTH+1, msgbuf, 13);
			}
			rwunlock(&FIDLOCK, "FIDLOCK auth");
			break;

		} default:
			sayerr(msgbuf, "unknown message type %d", msgbuf[4]);
			break;
		}

END:
		rlock(&TAGLOCK, "TAGLOCK end");
		lock(&info->alertlock, "alert end");
		if (info->alert == FLUSH) {
			uint2le(msgbuf+5, 2, info->tag);
			say(TFLUSH+1, msgbuf, 0);
			info->alert = NONE;
		}
		info->tag = NOTAG;
		unlock(&info->alertlock, "alert end");
		rwunlock(&TAGLOCK, "TAGLOCK end");
	}
}

void
get(char *dest, size_t len, char *buf)
{
	size_t r = fread(dest, 1, len, stdin);
	if (ferror(stdin)) {
		char *err = strerr(errno);
		sayerr(buf, "error reading input");
		die("sd9p: fread: %s", err);
	}
	if (r < len) {
		if (r || (buf != dest))  // Ended in middle of a message
			sayerr(buf, "short read - expected at least %d, got %d", len, r);
		exit(EXIT_SUCCESS);
	}
}

int
main(int argc, char **argv)
{
	setbuf(stdout, NULL);
	if (signal(SIGUSR1, sigusr1) == SIG_ERR)
		die("sd9p: signal: %s", strerror(errno));
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
			MSGBUF[5]=MSGBUF[6]=0xFF, get(MSGBUF+4, msglen-4, MSGBUF);
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
			if ((msglen < 7+6) || (msglen < 7+6+le2uint(MSGBUF+7+4, 2))) {
				sayerr(MSGBUF, "partial version message");
				continue;
			}
			uint16_t vlen = le2uint(MSGBUF+7+4, 2);
			if ((vlen < 6) || strncmp(MSGBUF+7+6, "9P2000", 6) ||
					((vlen > 6) && (MSGBUF[7+6+6] != '.'))) {
				strncpy(MSGBUF+7+6, "unknown", 7);
				uint2le(MSGBUF+7+4, 2, 7);
				say(TVERSION+1, MSGBUF, 4+2+7);
				continue;
			}
			if (le2uint(MSGBUF+7, 4) < MINLEN) {
				sayerr(MSGBUF, "message size too small");
				continue;
			}
			if (!(MSGBUF = realloc(MSGBUF, MSGLEN=le2uint(MSGBUF+7, 4))))
				die("sd9p: realloc: %s", strerr(errno));
			for (int i=0; i<THREADC; i++) {
				lock(&threads[i].alertlock, "alert in worker");
				threads[i].alert = ABORT;
				unlock(&threads[i].alertlock, "alert in worker");
				if ((res=pthread_kill(threads[i].id, SIGUSR1)))
					die("sd9p: pthread_kill %d: %s", i, strerr(res));
			}
			wlock(&FIDLOCK, "FIDLOCK reset");
			rlock(&QIDLOCK, "QIDLOCK fid reset");
			for (int i=0; i<MAXFIDS; i++) {
				if (FIDS[i].fid == NOFID)
					continue;
				if (QIDS[FIDS[i].qid].type & QTAUTH) {
					if (close(FIDS[i].info.auth.w) || close(FIDS[i].info.auth.r))
						die("sd9p: close auth fds: %s", strerr(errno));
					if (FIDS[i].info.auth.pidstatus > 1) {
						if (kill(FIDS[i].info.auth.pidstatus, SIGTERM))
							die("sd9p: kill %ld: %s", FIDS[i].info.auth.pidstatus,
								strerr(errno));
			uint2le(MSGBUF+7+4, 2, 6);
			say(TVERSION+1, MSGBUF, 4+2+6);

		// Handle flush message
		} else if (MSGBUF[4] == TFLUSH) {
			uint16_t tag = le2uint(MSGBUF+5, 2);
			if (tag == NOTAG) {
				say(TFLUSH+1, MSGBUF, 0);
				continue;
			}
			int found = 0;
			wlock(&TAGLOCK, "TAGLOCK send flush");
			if (!found)
				say(TFLUSH+1, MSGBUF, 0);
			for (int i=0; i<THREADC; i++)
				if (threads[i].tag == tag) {
					found=1;
					lock(&threads[i].alertlock, "alert flush to worker");
					threads[i].alert = FLUSH;
					threads[i].tag = tag;
					unlock(&threads[i].alertlock, "alert flush to worker");
					if ((res=pthread_kill(threads[i].id, SIGUSR1)))
						die("sd9p: pthread_kill %d: %s", i, strerr(res));
					break;
				}
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
