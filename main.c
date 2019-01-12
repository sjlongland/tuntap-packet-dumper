#include "linuxtun.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(void) {
	fd_set rfds;
	struct timeval tv;
	struct tundev_t tun;
	int res;

	/* Open a tunnel device */
	memset(&tun, 0, sizeof(tun));
	res = tun_open(&tun);
	if (res < 0) {
		fprintf(stderr, "Failed to open tunnel: %s\n",
				strerror(-res));
		return 1;
	}

	while (1) {
		/* Wait for the next packet (up to 5 seconds) */
		FD_ZERO(&rfds);
		FD_SET(tun.fd, &rfds);

		tv.tv_sec = 5;
		tv.tv_usec = 0;
		res = select(tun.fd + 1, &rfds, NULL, NULL, &tv);
		if (res < 0) {
			fprintf(stderr, "select fails: %s\n",
					strerror(errno));
			break;
		} else if (res > 0) {
			/* We have a packet */
			union {
				uint8_t raw[1508];
				struct tundev_frame_t data;
			} frame;
			const uint8_t* ptr = frame.data.data;
			int off = 0;
			int len = tun_read(&tun, &frame.data, sizeof(frame));
			if (len < 0) {
				fprintf(stderr, "read fails: %s\n",
						strerror(-len));
				break;
			}

			/* Dump the frame data out to stdout */
			printf("Flags: 0x%04x  Protocol: 0x%04x\n",
					frame.data.info.flags,
					frame.data.info.proto);
			printf("%4d:  0  1  2  3  4  5  6  7"
				   "  8  9 10 11 12 13 14 15", len);
			while (len) {
				if (!(off % 16))
					printf("\n%4d:", off);

				printf(" %02x", *ptr);
				ptr++;
				off++;
				len--;
			}
			printf("\n");
		}
	}

	/* Close the tunnel */
	res = tun_close(&tun);
	if (res < 0) {
		fprintf(stderr, "Close fails: %s\n",
				strerror(-res));
		return 1;
	}

	return 0;
}
