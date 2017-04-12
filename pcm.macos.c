#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <AudioToolbox/AudioQueue.h>
#include <CoreAudio/CoreAudioTypes.h>


int BUFLEN = 4096;
struct sync {
	int done;
	pthread_mutex_t readlock;
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

void
setnonblockordie(int nonblock)
{
	int flags = fcntl(STDIN_FILENO, F_GETFL);
	if (flags == -1)
		die("pcm: fnctl F_GETFL: %s", strerror(errno));
	flags = nonblock ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
	if (fcntl(STDIN_FILENO, F_SETFL, flags) == -1)
		die("pcm: fnctl F_SETFL: %s", strerror(errno));
}

int
loadbuf(AudioQueueBufferRef buf, AudioQueueRef queue, pthread_mutex_t *readlock)
{
	// Get lock on reading from stdin
	int res = pthread_mutex_lock(readlock);
	if (res)
		die("pcm: pthread_mutex_lock: %s", strerror(res));

	// Read into the queue buffer
	ssize_t readc = read(STDIN_FILENO, buf->mAudioData, BUFLEN);
	OSStatus osres;
	// Pause until samples are available
	if ((readc == -1) && (errno == EAGAIN)) {
		setnonblockordie(0);
		if ((osres = AudioQueuePause(queue)))
			die("pcm: AudioQueuePause: OSStatus %d", osres);
		readc = read(STDIN_FILENO, buf->mAudioData, BUFLEN);
		if ((osres = AudioQueueStart(queue, NULL)))
			die("pcm: AudioQueueStart: OSStatus %d", osres);
		setnonblockordie(1);
	} 
	if (readc == -1)
		die("pcm: read: %s", strerror(errno));
	// End of input
	else if (readc == 0) {
		if ((res = pthread_mutex_unlock(readlock)))
			die("pcm: pthread_mutex_unlock: %s", strerror(res));
		return 1;
	}
	buf->mAudioDataByteSize = readc;

	// Load the read samples
	if ((osres = AudioQueueEnqueueBuffer(queue, buf, 0, NULL)))
		die("pcm: AudioQueueEnqueueBuffer: OSStatus %d", osres);

	// Free lock on reading from stdin
	if ((res = pthread_mutex_unlock(readlock)))
		die("pcm: pthread_mutex_unlock: %s", strerror(res));
	return 0;
}

void
callback(void *info_, AudioQueueRef queue, AudioQueueBufferRef buf)
{
	struct sync *info = info_;
	if ((!info->done) && loadbuf(buf, queue, &info->readlock)) {
		info->done = 1;
		AudioQueueStop(queue, 0);
		AudioQueueDispose(queue, 0);
		CFRunLoopStop(CFRunLoopGetCurrent());
	}
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		die("usage: pcm channels");
	int channels = strtol(argv[1], NULL, 10);
	if (channels <= 0)
		die("pcm: channels must be a positive integer");
	setnonblockordie(1);

	AudioStreamBasicDescription format = {
		.mSampleRate = 44100,
		.mFormatID = kAudioFormatLinearPCM,
		.mFormatFlags = kAudioFormatFlagIsSignedInteger,
		.mBytesPerPacket = 2 * channels,
		.mFramesPerPacket = 1,
		.mBytesPerFrame = 2 * channels,
		.mChannelsPerFrame = channels,
		.mBitsPerChannel = 16
	};
	struct sync info = { .done = 0 };
	int res = pthread_mutex_init(&info.readlock, NULL);
	if (res)
		die("pcm: pthread_mutex_init: %s", strerror(res));
	AudioQueueRef queue;
	CFRunLoopRef thisthread = CFRunLoopGetCurrent();
	OSStatus osres = AudioQueueNewOutput(&format, callback, &info, thisthread, NULL,
		0, &queue);
	if (osres)
		die("pcm: AudioQueueNewOutput: OSStatus %d", osres);
	int hasdata = 0;
	for (int i=0; i<2; i++) {
		AudioQueueBufferRef buf;
		if ((osres = AudioQueueAllocateBuffer(queue, BUFLEN, &buf)))
			die("pcm: AudioQueueAllocateBuffer: OSStatus %d", osres);
		hasdata = !loadbuf(buf, queue, &info.readlock) || hasdata;
	}
	if (hasdata) {
		if ((osres = AudioQueueStart(queue, NULL)))
			die("pcm: AudioQueueStart: OSStatus %d", osres);
		CFRunLoopRun();
	}
}
