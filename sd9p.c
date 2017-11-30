//TODO: use str functions where useful
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
#include <sys/wait.h>
#include <unistd.h>


const int THREADC = 16;
#define MAXFIDS 64
const int MINLEN = 256;  // >= 237 (largest rwalk message)
enum msg {
	TVERSION = 100,
	TAUTH = 102,
	TATTACH = 104,
	TFLUSH = 108,
	TWALK = 110,
	TOPEN = 112,
	TCREATE = 114,
	TREAD = 116,
	TWRITE = 118,
	TCLUNK = 120,
	TREMOVE = 122,
	TSTAT = 124,
	TWSTAT = 126
};
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
	char *uname, *aname;
};
struct file {
	int fd, pfd;
	uint64_t aqid;
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

/* printf to stderr with a newline, then exit with nonzero status. */
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

/* Write to buf len bytes of d in little endian. */
void
uint2le(char *buf, int len, uint64_t d)
{
	for (int i=0; i<len; i++)
		buf[i] = d >> 8*i;
}

/* Get the strerror string for errnum (only works once). */
char*
strerr(int errnum)
{
	int res = pthread_mutex_lock(&STRERR);  // assuming about to die
	if (res)
		die("sd9p: pthread_mutex_lock STRERR: errnum %d after %d", res, errnum);
	return strerror(errnum);
}

/* Lock lock described by s. */
void
lock(pthread_mutex_t *lock, char *s)
{
	int res = pthread_mutex_lock(lock);
	if (res)
		die("sd9p: pthread_mutex_lock %s: %s", s, strerr(res));
}

/* Unlock lock described by s. */
void
unlock(pthread_mutex_t *lock, char *s)
{
	int res = pthread_mutex_unlock(lock);
	if (res)
		die("sd9p: pthread_mutex_unlock %s: %s", s, strerr(res));
}

/* Output type 9p message stored in buf of bodylen length. The message should
   start 7 bytes into buf, and bytes 6-7 should be the tag. */
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

/* Return the little endian value stored in buf of length len. */
uint64_t
le2uint(char *buf, int len)
{
	uint64_t d = 0;
	for (int i=0; i<len; i++)
		d += buf[i] << 8*i;
	return d;
}

/* Output a 9p error message using buf by sprintf'ing the rest of the args. */
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

/* Parse 9p message msg with body format fmt and store the values in the rest of
   the args. fmt is a string where digits are a little endian number of that
	 length and s is a 9p string. */
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

/* Simplify the path aname. */
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

/* Lock lock described by s for writing. */
void
wlock(pthread_rwlock_t *lock, char *s)
{
	int res = pthread_rwlock_wrlock(lock);
	if (res)
		die("sd9p: pthread_rwlock_wrlock %s: %s", s, strerr(res));
}

/* Return the index of fid in the global fid array, or the number of current
   fids if it isn't found. The fid array lock should be held. */
int
getifid(uint32_t fid)
{
	int ifidunused = MAXFIDS;
	for (int ifid=0; ifid<MAXFIDS; ifid++)
		if (FIDS[ifid].fid == fid)
			return ifid;
		else if ((ifidunused == MAXFIDS) && (FIDS[ifid].fid == NOFID))
			ifidunused = ifid;
	return ifidunused;
}

/* Unlock lock described by s. */
void
rwunlock(pthread_rwlock_t *lock, char *s)
{
	int res = pthread_rwlock_unlock(lock);
	if (res)
		die("sd9p: pthread_rwlock_unlock %s: %s", s, strerr(res));
}

/* Return the qid of the file identified by buf, or the number of qids if no
   match is found. The qid array's lock should be held. */
uint64_t
matchqid(struct stat *buf)
{
	uint64_t qid;
	for (qid=0; qid<QIDC; qid++)
		if ((QIDS[qid].st_dev == buf->st_dev) && (QIDS[qid].st_ino == buf->st_ino))
			break;
	return qid;
}

/* Lock lock described by s for reading. */
void
rlock(pthread_rwlock_t *lock, char *s)
{
	int res = pthread_rwlock_rdlock(lock);
	if (res)
		die("sd9p: pthread_rwlock_rdlock %s: %s", s, strerr(res));
}

/* Get the qid for the file identified by buf of 9p type, possibly creating a
   new entry. The qid array lock shouldn't be held by the calling thread. */
uint64_t
getqid(struct stat *buf, uint8_t type)
{
	// rlock QIDLOCK??? TODO: see when added/removed?
	rlock(&QIDLOCK, "QIDLOCK getqid start");
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

/* Write the entry for qid to buf. */
void
wqid(uint64_t qid, char *buf)
{
	rlock(&QIDLOCK, "QIDLOCK wqid");
	uint2le(buf, 1, QIDS[qid].type);
	uint2le(buf+1, 4, QIDS[qid].version);
	uint2le(buf+1+4, 8, qid);
	rwunlock(&QIDLOCK, "QIDLOCK wqid");
}

/* Output a 9p error message like 's: strerror(errnum)' using buf. */
void
saystrerr(char *s, int errnum, char *buf)
{
	char errs[128];
	if (strerror_r(errnum, errs, sizeof(errs)))
		snprintf(errs, sizeof(errs), "errnum %d (long error message)", errnum);
	sayerr(buf, "%s: %s", s, errs);
}

/* Write s into p, reallocating p if necessary. */
void
copystring(char *s, char **p)
{
	int slen = strlen(s);
	if ((!p || (slen > strlen(*p))) && !(*p = realloc(*p, slen+1)))
		die("sd9p: realloc copystring (%d): %s", slen, strerr(errno));
	strcpy(*p, s);  //TODO: order?
}

/* Loop forever handling 9p messages. */
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
			int ifid = getifid(afid);
			if (ifid == MAXFIDS)
				sayerr(msgbuf, "maximum fids reached (%d)", MAXFIDS);
			else if (FIDS[ifid].fid != NOFID)
				sayerr(msgbuf, "fid %d already in use", afid);
			else {
				FIDS[ifid].fid = afid;
				FIDS[ifid].i.auth.uname = uname;
				FIDS[ifid].i.auth.aname = aname;
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
				if ((FIDS[ifid].i.auth.pidstatus = fork()) == -1)
					die("sd9p: fork: %s", strerr(errno));
				if (!FIDS[ifid].i.auth.pidstatus) {
					if ((dup2(fds[1],1)==-1) || (dup2(fds[2],0)==-1))
						die("sd9p: dup2: %s", strerror(errno));
					if (execvp(ARGV[2], ARGV+2) == -1)
						die("sd9p: execvp: %s", strerror(errno));
				}
				if (close(fds[1]) || close(fds[2]))
					die("sd9p: close pipe: %s", strerr(errno));
				FIDS[ifid].i.auth.r = fds[0], FIDS[ifid].i.auth.w = fds[3];
				struct stat buf;
				if (fstat(FIDS[ifid].i.auth.r, &buf))
					die("sd9p: fstat auth r: %s", strerr(errno));
				FIDS[ifid].qid = getqid(&buf, QTAUTH);
				wqid(FIDS[ifid].qid, msgbuf+7);
				say(TAUTH+1, msgbuf, 13);
			}
			rwunlock(&FIDLOCK, "FIDLOCK auth");
AUTHFREE:
			free(uname), free(aname);
			break;

		} case TATTACH: {
			uint32_t fid, afid;
			char *uname=NULL, *aname=NULL;
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
				int ifid = getifid(afid);
				rlock(&QIDLOCK, "QIDLOCK attach afid check");
				if ((ifid == MAXFIDS) || (FIDS[ifid].fid == NOFID))
					err=1, sayerr(msgbuf, "afid not found");
				else if (!(QIDS[FIDS[ifid].qid].type & QTAUTH))
					err=1, sayerr(msgbuf, "afid not open for authentication");
				else if (strcmp(uname, FIDS[ifid].i.auth.uname))
					err=1, sayerr(msgbuf, "uname doesn't match afid's uname");
				else if (strcmp(aname, FIDS[ifid].i.auth.aname))
					err=1, sayerr(msgbuf, "aname doesn't match afid's aname");
				else if (FIDS[ifid].i.auth.pidstatus) {
					if (FIDS[ifid].i.auth.pidstatus > 1) {
						pid_t pid = FIDS[ifid].i.auth.pidstatus;
						int statloc;
						if (((res = waitpid(pid, &statloc, WNOHANG))) == -1)
							die("sd9p: waitpid %ld attach: %s", pid, strerr(errno));
						if (res && WIFEXITED(statloc))
							FIDS[ifid].i.auth.pidstatus = !!WEXITSTATUS(statloc);
						else if (res && WIFSIGNALED(statloc))
							FIDS[ifid].i.auth.pidstatus = 1;
					}
					if (FIDS[ifid].i.auth.pidstatus > 1)
						err=1, sayerr(msgbuf, "authentication incomplete");
					else if (FIDS[ifid].i.auth.pidstatus)
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
			int err = errno;
			lock(&info->alertlock, "alertlock attach open");
			if (info->alert != NONE)
				goto ATTACHFREE;
			if (fd == -1) {
				if (err == EACCES)
					sayerr(msgbuf, "authenticated, but permission denied");
				else if ((err == ENOENT) || (err == ENOTDIR))
					sayerr(msgbuf, "aname directory does not exist");
				else
					saystrerr("open", err, msgbuf);
				goto ATTACHFREE;
			}
			unlock(&info->alertlock, "alertlock attach fstat");
			struct stat buf;
			res = fstat(fd, &buf);
			lock(&info->alertlock, "alertlock attach fstat");
			if (info->alert != NONE)
				goto ATTACHFREE;
			if (res == -1) {
				saystrerr("stat", errno, msgbuf);
				goto ATTACHFREE;
			}
			wlock(&FIDLOCK, "FIDLOCK attach entry");
			int ifid = getifid(fid);
			if (ifid == MAXFIDS)
				sayerr(msgbuf, "maximum fids reached (%d)", MAXFIDS);
			else if (FIDS[ifid].fid != NOFID)
				sayerr(msgbuf, "fid %d already in use", fid);
			else {
				FIDS[ifid].fid = fid;
				FIDS[ifid].qid = getqid(&buf, 0);
				FIDS[ifid].offset = 0;
				FIDS[ifid].i.f.fd = -1;
				FIDS[ifid].i.f.pfd = fd;
				FIDS[ifid].i.f.aqid = FIDS[ifid].qid;
				FIDS[ifid].i.f.name=NULL, copystring("/", &FIDS[ifid].i.f.name);
				FIDS[ifid].i.f.dirent = NULL;
				wqid(FIDS[ifid].qid, msgbuf+7);
				say(TATTACH+1, msgbuf, 13);
			}
			rwunlock(&FIDLOCK, "FIDLOCK attach entry");
ATTACHFREE:
			free(aname), free(uname);
			if (fd != -1)
				close(fd);
			break;

		} case TWALK: {
			uint32_t fid, newfid;
			uint16_t nwname;
			if (parsemsg(msgbuf, "442", &fid, &newfid, &nwname))
				goto END;
			if (nwname > 16) {
				sayerr(msgbuf, "nwname > 16 (%u)", nwname);
				goto END;
			}
			char **wnames = calloc(nwname, sizeof(char*));
			if (!wnames)
				die("sd9p: calloc %u wnames: %s", nwname, strerr(errno));
			int pfd = -1;
			char *fname = NULL;
			uint32_t msglen = le2uint(msgbuf, 4);
			uint32_t imsg = 7+4+4+2;
			for (int iwname=0; iwname<nwname; iwname++) {
				uint32_t msgleft = msglen - imsg;
				uint16_t slen;
				if ((msgleft < 2) || (msgleft-2 < (slen=le2uint(msgbuf+(imsg+=2), 2)))) {
					sayerr(msgbuf, "short message (in wnames)");
					goto WALKFREE;
				}
				char *s = wnames[iwname] = malloc(slen+1);
				if (!s)
					die("sd9p: malloc wname (len %u): %s", slen, strerr(errno));
				*stpncpy(s, msgbuf+imsg, slen) = '\0';
				imsg += slen;
				if ((s[0] == '.') && (!s[1] || (s[1] == '/'))) {
					sayerr(msgbuf, "wname '.' does not exist in 9p");
					goto WALKFREE;
				}
				for (uint16_t is=0; is<(slen && slen-1); is++)
					if (s[is] == '/') {
						sayerr(msgbuf, "wname can't have non-ultimate / (%s)", s);
						goto WALKFREE;
					}
				if (!strcmp(s, "../"))
					s[2] = '\0';
			}
			int err=0;
			rlock(&FIDLOCK, "FIDLOCK walk fid");
			rlock(&QIDLOCK, "QIDLOCK walk fid");
			int ifid = getifid(fid);
			uint64_t qid, aqid;
			if ((ifid == MAXFIDS) || (FIDS[ifid].fid == NOFID))
				err=1, sayerr(msgbuf, "fid %u not found", fid);
			else if (QIDS[FIDS[ifid].qid].type & QTAUTH)
				err=1, sayerr(msgbuf, "can't walk auth fid (%u)", fid);
			else if (FIDS[ifid].i.f.fd != -1)
				err=1, sayerr(msgbuf, "walk fid opened (%u)", fid);
			else {
				qid = FIDS[ifid].qid;
				aqid = FIDS[ifid].i.f.aqid;
        if ((pfd = dup(FIDS[ifid].i.f.pfd)) == -1)
					die("sd9p: dup pfd: %s", strerr(errno));
				copystring(FIDS[ifid].i.f.name, &fname);
			}
			rwunlock(&QIDLOCK, "QIDLOCK walk fid");
			rwunlock(&FIDLOCK, "FIDLOCK walk fid");
			if (err)
				goto WALKFREE;
			int nwqid;
			for (nwqid=0; nwqid<nwname; nwqid++) {
				char *newpfdname = nwqid ? fname : wnames[nwqid-1];
				if (!strcmp(newpfdname, "..")) {
					unlock(&info->alertlock, "alertlock walk fstat");
					struct stat buf;
					if (fstat(pfd, &buf))
						die("sd9p: fstat walk: %s", strerr(errno));
					lock(&info->alertlock, "alertlock walk fstat");
					if (info->alert != NONE)
						goto WALKFREE;
					if (getqid(&buf, 0) == aqid)
						newpfdname = ".";
				}
				unlock(&info->alertlock, "alertlock walk fopenat");
				int tmp = openat(pfd, newpfdname, O_DIRECTORY|O_CLOEXEC|O_SEARCH);
				err = errno;
				lock(&info->alertlock, "alertlock walk openat");
				if (tmp != -1)
					close(pfd), pfd=tmp;
				if (info->alert != NONE)
					goto WALKFREE;
				if (tmp == -1) {
					if (nwqid)
						break;
					if (err == EACCES)
						sayerr(msgbuf, "permission denied (%s)", newpfdname);
					else if ((err == ENOENT) || (err == ENOTDIR))
						sayerr(msgbuf, "directory not found (%s)", newpfdname);
					else
						saystrerr("openat", err, msgbuf);
					goto WALKFREE;
				}
				unlock(&info->alertlock, "alertlock walk fstatat");
				struct stat buf;
				int res = fstatat(pfd, wnames[nwqid], &buf, 0);
				err = errno;
				lock(&info->alertlock, "alertlock walk fstatat");
				if (info->alert != NONE)
					goto WALKFREE;
				if (res == -1) {
					if (nwqid)
						break;
					if (err == EACCES)
						sayerr(msgbuf, "permission denied (%s)", wnames[nwqid]);
					else if (err == ENOENT)
						sayerr(msgbuf, "file not found (%s)", wnames[nwqid]);
					else
						saystrerr("fstatat", err, msgbuf);
					goto WALKFREE;
				}
				if ((qid=getqid(&buf, 0)) == aqid) {
					unlock(&info->alertlock, "alertlock walk openat /");
					tmp = openat(pfd, wnames[nwqid], O_DIRECTORY|O_CLOEXEC|O_SEARCH);
					lock(&info->alertlock, "alertlock walk openat /");
					if (tmp != -1)
						close(pfd), pfd=tmp;
					if (info->alert != NONE)
						goto WALKFREE;
					if (tmp == -1) {
						if (nwqid)
							break;
						if (err == EACCES)
							sayerr(msgbuf, "permission denied at / (%s)", newpfdname);
						else if ((err == ENOENT) || (err == ENOTDIR))
							sayerr(msgbuf, "directory not found at / (%s)", newpfdname);
						else
							saystrerr("openat at /", err, msgbuf);
						goto WALKFREE;
					}
				}
				wqid(qid, msgbuf+9+(13*nwqid));
			}
			if (nwqid == nwname) {
				err = 0;
				wlock(&FIDLOCK, "FIDLOCK walk newfid");
				int inewfid = getifid(newfid);
				if (inewfid == MAXFIDS)
					err=1, sayerr(msgbuf, "maximum fids reached (%d)", MAXFIDS);
				else if ((FIDS[inewfid].fid != NOFID) && (fid != newfid))
					err=1, sayerr(msgbuf, "newfid already in use (%d)", newfid);
				else {
					FIDS[inewfid].fid = newfid;
					FIDS[inewfid].qid = qid;
					if (FIDS[inewfid].i.f.fd != -1)
						close(FIDS[inewfid].i.f.fd);
					FIDS[inewfid].i.f.fd = -1;
					close(FIDS[inewfid].i.f.pfd);
					FIDS[inewfid].i.f.pfd = pfd;
					pfd = -1;
					FIDS[inewfid].i.f.aqid = aqid;
					free(FIDS[inewfid].i.f.name);
					char *name = nwqid ? wnames[nwqid-1] : fname;
					if (qid == aqid)
						FIDS[inewfid].i.f.name=NULL, copystring("/", &FIDS[inewfid].i.f.name);
					else {
						FIDS[inewfid].i.f.name = name;
						if (nwqid)
							wnames[nwqid-1] = NULL;
						else
							fname = NULL;
					}
					free(FIDS[inewfid].i.f.dirent);
					FIDS[inewfid].i.f.dirent = NULL;
				}
				rwunlock(&FIDLOCK, "FIDLOCK walk newfid");
				if (err)
					goto WALKFREE;
			}
			uint2le(msgbuf+7, 2, nwqid);
			say(TWALK+1, msgbuf, 2+(nwqid*13));
WALKFREE:
			if (pfd != -1)
				close(pfd);
			if (wnames)
				for (int i=0; i<nwname; i++)
					free(wnames[i]);
			free(wnames);
			free(fname);
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

/* Get the next message of length len into dest, responding about short reads
   using buf. */
void
get(size_t len, char *dest, char *buf)
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

/* waitpid on pid into statloc, exiting with nonzero status on error. */
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

	// Main loop
	while (!feof(stdin)) {

		// Read in message
		MSGBUF[5]=MSGBUF[6]=0xFF, get(4, MSGBUF, MSGBUF);
		uint32_t msglen = le2uint(MSGBUF, 4);
		if (msglen < 7) {
			sayerr(MSGBUF, "stub message (shorter than 7 bytes? wtf?)");
			continue;
		}
		if (!MSGLEN && (msglen > MINLEN)) {  // First message & longer than default
			if (!(MSGBUF = realloc(MSGBUF, msglen)))
				die("sd9p: realloc: %s", strerr(errno));
			get(msglen-4, MSGBUF+4, MSGBUF);
		} else if (MSGLEN && (msglen > MSGLEN)) {  // Message too long
			get(3, MSGBUF+4, MSGBUF);
			sayerr(MSGBUF, "message length %u > %u", msglen, MSGLEN);
			do get(MSGLEN-7,MSGBUF+7,MSGBUF); while ((msglen-=(MSGLEN-7))-7 > MSGLEN);
			get(msglen-7, MSGBUF+7, MSGBUF);
			continue;
		} else  // Message fits
			get(msglen-4, MSGBUF+4, MSGBUF);

		// First message must be version message
		if (!MSGLEN && (MSGBUF[4] != TVERSION)) {
			sayerr(MSGBUF, "version message required");
			continue;
		}

		// Handle version messages
		if (MSGBUF[4] == TVERSION) {

			// Get protocol and message length
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

			// Reset worker threads
			for (int i=0; i<THREADC; i++) {
				lock(&threads[i].alertlock, "alert abort");
				threads[i].alert = ABORT;
				unlock(&threads[i].alertlock, "alert abort");
				if ((res=pthread_kill(threads[i].id, SIGUSR1)))
					die("sd9p: pthread_kill %d: %s", i, strerr(res));
			}

			// Clear FIDs
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
					close(FIDS[i].i.f.pfd);
					if (FIDS[i].i.f.fd != -1)
						close(FIDS[i].i.f.fd);
					free(FIDS[i].i.f.name);
					free(FIDS[i].i.f.dirent);
				}
				FIDS[i].fid = NOFID;
			}
			rwunlock(&QIDLOCK, "QIDLOCK fid reset");
			rwunlock(&FIDLOCK, "FIDLOCK reset");

			// Reply to successful version message
			uint2le(MSGBUF+7+4, 2, 6);
			say(TVERSION+1, MSGBUF, 4+2+6);

		// Handle flush message
		} else if (MSGBUF[4] == TFLUSH) {

			// Get tag to flush
			uint16_t tag;
			if ((msglen < 9) || ((tag=le2uint(MSGBUF+7,2)) == NOTAG)) {
				say(TFLUSH+1, MSGBUF, 0);
				continue;
			}

			// Signal worker with matching tag
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
