#include <errno.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>


/* Read len characters from socketfd into buf. If a read error occurs, output
 * "scstream: read <diemsg>: <errno error message>"
 * to stderr, close socketfd, and exit with nonzero status. */
void
readordie(int socketfd, uint8_t *buf, unsigned int len, char *diemsg)
{
	unsigned int r = 0;
	int thisr;
	while ((((thisr = read(socketfd, &buf[r], len - r)) >= 0) || (errno == EINTR)) &&
			((r += thisr) < len));
	if (thisr < 0) {
		int olderrno = errno;
		fputs("scstream: read ", stderr);
		errno = olderrno;
		perror(diemsg);
		close(socketfd);
		exit(EXIT_FAILURE);
	}
}

int
main()
{
	// Open connection to soundcloud
	struct addrinfo *scinfo;
	struct addrinfo hints = {0};
	hints.ai_protocol = IPPROTO_TCP;
	int gaires = getaddrinfo("api-v2.soundcloud.com", "443", &hints, &scinfo);
	if (gaires) {
		fprintf(stderr, "scstream: getaddrinfo looking up soundcloud: %s\n", gai_strerror(gaires));
		return EXIT_FAILURE;
	}
	int socketfd = socket(scinfo->ai_family, scinfo->ai_socktype, scinfo->ai_protocol);
	if (socketfd < 0) {
		perror("scstream: socket creation");
		freeaddrinfo(scinfo);
		return EXIT_FAILURE;
	}
	if (connect(socketfd, scinfo->ai_addr, scinfo->ai_addrlen)) {
		perror("scstream: connect");
		freeaddrinfo(scinfo);
		close(socketfd);
		return EXIT_FAILURE;
	}
	freeaddrinfo(scinfo);

	// Send TLS Hello
	uint32_t t = time(NULL);
	srand(t);
#define RAND7 rand(), rand(), rand(), rand(), rand(), rand(), rand()
#define RAND28 RAND7, RAND7, RAND7, RAND7
	uint8_t hello[] = {
		0x16,                    // TLS record type (handshake)
		0x03, 0x03,              // TLS version (1.2)
		0x00, 0x2d,              // handshake message length (45)
		0x01,                    // handshake message type (hello)
		0x00, 0x00, 0x29,        // hello length (41)
		0x03, 0x03,              // TLS version (1.2)
		t>>24, t>>16, t>>8, t,   // seconds since epoch
		RAND28,                  // 28 random bytes
		0x00,                    // session ID (none)
		0x00, 0x02, 0x00, 0x05,  // cypher suite (TLS_RSA_WITH_RC4_128_SHA)
		0x01, 0x00               // compression methods (null)
	};
	unsigned int w = 0;
	int thisw;
	while ((((thisw = write(socketfd, &hello[w], sizeof(hello) - w)) >= 0) || (errno == EINTR)) &&
			((w += thisw) < sizeof(hello)));
	if (thisw < 0) {
		perror("scstream: write hello");
		close(socketfd);
		return EXIT_FAILURE;
	}

	// Read TLS Hello, Certificate, Hello Done
	uint8_t tvl[1+2+2];  // type, version, length
	readordie(socketfd, tvl, sizeof(tvl), "hello start");
	uint8_t serverhello[tvl[3]<<8 | tvl[4]];
	readordie(socketfd, serverhello, sizeof(serverhello), "hello body");
	readordie(socketfd, tvl, sizeof(tvl), "certificate start");
	uint8_t certmsg[tvl[3]<<8 | tvl[4]];
	readordie(socketfd, certmsg, sizeof(certmsg), "certificate body");
	readordie(socketfd, tvl, sizeof(tvl), "hello done start");
	uint8_t hellodone[tvl[3]<<8 | tvl[4]];
	readordie(socketfd, hellodone, sizeof(hellodone), "hello done body");

	// Get public key values from certificate
	uint8_t *cert = &certmsg[1+3+3+3];
	uint8_t *publickey = &cert[4+4+5+14+15+98+32+64+4+15+5];
	// The RSA modulus (n) is unsigned, but DER integers are signed, so the modulus is prefixed by a
	// leading zero byte in the certificate. This means it will be e.g. 257 bytes long if the modulus
	// is originally 256 bytes long. It would be simpler and equivalent to do the math as though the
	// modulus is actually that extra byte long and send a 257-byte message, but we might be expected
	// to send a 256-byte message (it's not in the spec, but that's what OpenSSL does). signedn refers
	// to the modulus including this leading zero byte while n refers to the modulus without it.
	// TODO: try ignoring this fact and send a 257-byte message with leading zero
	uint16_t signednlen = publickey[4+2]<<8 | publickey[4+2+1];
	uint8_t *signedn = &publickey[4+4];
	uint16_t nlen = signednlen - 1;
	uint8_t *n = &signedn[1];
	unsigned int e = n[nlen+2]<<16 | n[nlen+2+1]<<8 | n[nlen+2+2];

	// Send TLS Encrypted Pre Master Secret
	uint8_t pms[48] = {
		0x03, 0x03,                                           // TLS version (1.2)
		RAND28, RAND7, RAND7, rand(), rand(), rand(), rand()  // 46 random bytes
	};
	uint8_t encryptedpms[nlen] = {0};
	//uint8_t keyexchange[] = {
	//	0x16,  // TLS record type (handshake)
	//	0x03, 0x03,  // TLS version (1.2)
	//	0x00, 0x00,  // handshake message length ()
	//	0x10,  // handshake message type (key exchange)

	// Send HTTP request and read response
	char request[] = 
		"GET / HTTP/1.0\r\n"
		"host: example.com\r\n"
		"\r\n";
}
