#ifndef _LINUXTUN_H
#define _LINUXTUN_H

#include <stdint.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <errno.h>

struct tundev_t {
	/*! Tunnel interface name */
	char	name[IFNAMSIZ];
	/*! Tunnel file descriptor */
	int	fd;
};

struct tundev_frame_t {
	/*! Size of packet */
	size_t		sz;
	/*! Packet information header */
	struct tun_pi	info;
	/*! Packet data */
	uint8_t		data[];
};

int tun_open(struct tundev_t* const dev, int flags);
int tun_read(const struct tundev_t* const dev, struct tundev_frame_t*
		const frame, size_t max_sz);
int tun_close(struct tundev_t* const dev);

#endif
