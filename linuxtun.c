/* vim: set tw=78 ts=8 sts=8 noet fileencoding=utf-8: */
#include <linux/if.h>
#include <linux/if_tun.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include "linuxtun.h"

static int tun_alloc(char* const dev);

int tun_open(struct tundev_t* const dev) {
	if (dev->fd > 0)
		return -EALREADY;

	dev->fd = tun_alloc(dev->name);
	if (dev->fd < 0)
		return -errno;

	return 0;
}

int tun_close(struct tundev_t* const dev) {
	if (dev->fd <= 0)
		return -EALREADY;

	int res = close(dev->fd);
	if (res < 0)
		return -errno;

	dev->fd = 0;
	dev->name[0] = 0;
	return 0;
}

int tun_read(const struct tundev_t* const dev, struct tundev_frame_t*
		const frame, size_t max_sz) {
	uint8_t buffer[max_sz];
	uint8_t* ptr = buffer;
	ssize_t len = read(dev->fd, buffer, max_sz);
	if (len < sizeof(frame->info)) {
		if (errno)
			return -errno;
		else
			return -EPIPE;
	}

	/* First four bytes are the packet information */
	memcpy(&(frame->info), ptr, sizeof(frame->info));
	ptr += sizeof(frame->info);
	len -= sizeof(frame->info);

	/* Protocol is in big-endian format */
	frame->info.proto = ntohs(frame->info.proto);

	/* Rest is the packet data */
	memcpy(frame->data, ptr, len);
	frame->sz = len;

	return len;
}

/* Credit: Documentation/networking/tuntap.txt in the Linux kernel sources */
static int tun_alloc(char* const dev) {
	struct ifreq ifr;
	int fd, err;

	fd = open("/dev/net/tun", O_RDWR);
	if (fd < 0)
		return fd;

	memset(&ifr, 0, sizeof(ifr));

	/* Flags: IFF_TUN   - TUN device (no Ethernet headers) 
	 *        IFF_TAP   - TAP device  
	 *
	 *        IFF_NO_PI - Do not provide packet information  
	 */ 
	ifr.ifr_flags = IFF_TUN; 
	if (*dev)
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	err = ioctl(fd, TUNSETIFF, (void *) &ifr);
	if (err < 0){
		close(fd);
		return err;
	}
	strcpy(dev, ifr.ifr_name);
	return fd;
}
