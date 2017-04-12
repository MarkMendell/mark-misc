#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


unsigned long
getunicodeordie(char *buf)
{
	size_t read = fread(buf, 1, 4, stdin);
	if (ferror(stdin)) {
		perror("fread");
		exit(EXIT_FAILURE);
	} else if (read < 4) {
		fputs("jdecode: 4 hex digits required after \\u\n", stderr);
		exit(EXIT_FAILURE);
	}
	char *endptr;
	errno = 0;
	unsigned long unicode = strtol(buf, &endptr, 16);
	if (errno) {
		perror("jdecode: strtol");
		exit(EXIT_FAILURE);
	}
	if (endptr != &buf[4]) {
		fprintf(stderr, "jdecode: bad unicode sequence '%s'\n", buf);
		exit(EXIT_FAILURE);
	}
	return unicode;
}

int
main(void)
{
	int c;
	int escaped = 0;
	char buf[5];
	buf[4] = '\0';
	unsigned long long unicode;
	while ((c = getchar()) != EOF) {
		if (escaped)
			switch(c) {
				case 'b':
					putchar('\b');
					break;
				case 'f':
					putchar('\f');
					break;
				case 'n':
					putchar('\n');
					break;
				case 'r':
					putchar('\r');
					break;
				case 't':
					putchar('\t');
					break;
				case 'u':
					unicode = getunicodeordie(buf);
					if ((unicode >= 0xd800) && (unicode <= 0xdbff)) {
						size_t read = fread(buf, 1, 2, stdin);
						if (ferror(stdin)) {
							perror("jdecode: fread");
							return EXIT_FAILURE;
						}
						if (read < 2) {
							fputs("jdecode: need 2nd unicode after surrogate\n", stderr);
							return EXIT_FAILURE;
						}
						unsigned long unicode2 = getunicodeordie(buf);
						unicode = 0x10000 + (((unicode & 0x3ff)<<10) | (unicode2 & 0x3ff));
					}
					if (unicode <= 0x7f)
						fwrite((unsigned char*)&unicode, 1, 1, stdout);
					else if (unicode <= 0x7ff) {
						buf[0] = 0xc0 | (unsigned char)(unicode >> 6);
						buf[1] = 0x80 | (unsigned char)(unicode & 0x3f);
						fwrite(buf, 1, 2, stdout);
					} else if (unicode <= 0xffff) {
						buf[0] = 0xe0 | (unsigned char)(unicode >> 12);
						buf[1] = 0x80 | (unsigned char)(unicode>>6 & 0x3f);
						buf[2] = 0x80 | (unsigned char)(unicode & 0x3f);
						fwrite(buf, 1, 3, stdout);
					} else {
						buf[0] = 0xf0 | (unsigned char)(unicode >> 18);
						buf[1] = 0x80 | (unsigned char)(unicode>>12 & 0x3f);
						buf[2] = 0x80 | (unsigned char)(unicode>>6 & 0x3f);
						buf[3] = 0x80 | (unsigned char)(unicode & 0x3f);
						fwrite(buf, 1, 4, stdout);
					}
					if (ferror(stdout)) {
						perror("jdecode: fwrite unicode");
						return EXIT_FAILURE;
					}
					break;
				default:
					putchar(c);
					break;
			}
		else if (c != '\\')
			putchar(c);
		if (ferror(stdout)) {
			perror("putchar");
			return EXIT_FAILURE;
		}
		escaped = (c == '\\') && !escaped;
	}
	if (ferror(stdin)) {
		perror("getchar");
		return EXIT_FAILURE;
	}
}
