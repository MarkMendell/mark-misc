#include <stdio.h>
#include <stdlib.h>

#include <AudioToolbox/AudioQueue.h>
#include <CoreAudio/CoreAudioTypes.h>


static int BUFLEN = 4096;


void
osstatusdie(char *function, OSStatus res)
{
	fprintf(stderr, "pcm: %s: OSStatus %d\n", function, res);
	exit(EXIT_FAILURE);
}

int
loadbuf(AudioQueueBufferRef buf, AudioQueueRef queue)
{
	buf->mAudioDataByteSize = fread(buf->mAudioData, 1, BUFLEN, stdin);
	if (ferror(stdin)) {
		perror("pcm: fread");
		exit(EXIT_FAILURE);
	} else if (buf->mAudioDataByteSize == 0)
		return 1;
	OSStatus res = AudioQueueEnqueueBuffer(queue, buf, 0, NULL);
	if (res)
		osstatusdie("AudioQueueEnqueueBuffer", res);
	return 0;
}

void
callback(void *_, AudioQueueRef queue, AudioQueueBufferRef buf)
{
	if (loadbuf(buf, queue)) {
		AudioQueueStop(queue, 0);
		AudioQueueDispose(queue, 0);
		CFRunLoopStop(CFRunLoopGetCurrent());
	}
}

int
main(int argc, char **argv)
{
	int channels = 1;
	if ((argc > 1) && ((channels = strtol(argv[1], NULL, 10)) <= 0)) {
		fputs("pcm: channels must be a positive integer\n", stderr);
		return EXIT_FAILURE;
	}

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
	AudioQueueRef queue;
	CFRunLoopRef thisthread = CFRunLoopGetCurrent();
	OSStatus res = AudioQueueNewOutput(&format, callback, NULL, thisthread, NULL, 0, &queue);
	if (res)
		osstatusdie("AudioQueueNewOutput", res);
	int hasdata = 0;
	for (int i=0; i<2; i++) {
		AudioQueueBufferRef buf;
		if ((res = AudioQueueAllocateBuffer(queue, BUFLEN, &buf)))
			osstatusdie("AudioQueueAllocateBuffer", res);
		hasdata = !loadbuf(buf, queue) || hasdata;
	}
	if (hasdata) {
		if ((res = AudioQueueStart(queue, NULL)))
			osstatusdie("AudioQueueStart", res);
		CFRunLoopRun();
	}
}
