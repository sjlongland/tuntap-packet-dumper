# Simple packet dump utility

This just reads packets from a tun device and prints them out on the console.
It is known to work with Linux kernel 4.15.  The intent of this program is to
explore how packets can be read and dissected from the tap interface.

## TUN/TAP frame format (for `tun` devices)

The TUN/TAP interface basically spits out raw frames to you.  You have the
option of having the kernel pre-pend a "packet information" header to this.
Many examples I've seen "on the net" tell you to set the `IFF_NO_PI` flag when
opening the device without any explanation as to why you'd want to reject this
information.  I see that as some sort of knee-jerk reaction.

It turns out it's not so difficult to understand, but the standard [TUN/TAP
docs](http://git.kernel.org/?p=linux/kernel/git/torvalds/linux-2.6.git;a=blob;f=Documentation/networking/tuntap.txt;hb=HEAD)
don't make it that clear how to decode that information.

Firstly, receiving frames is just done with the usual `read()` command.  You
use the usual mechanisms to wait for traffic to arrive on the file descriptor
(i.e. `select()`), then when it arrives, you `read()` the entire packet in one
gulp.

That implies your buffer will need to be big enough to hold the packet *and*
the packet information header that TUN/TAP prepends.

Next, you'll want to look at the four byte information header.  This is a
`struct tun_pi` (see `linux/if_tun.h`).


```
  ----------- flags ------------   ------------ proto -----------
 /                              \ /                              \
+--------------------------------+--------------------------------+
|   Reserved for future use    |S|          Ethertype ID          |
+--------------------------------+--------------------------------+
                                |
                    TUN_PKT_STRIP
```

### `flags`

This is a 16-bit register, given in host-endian format (i.e. little-endian on
x86, etc) which is used to flag various conditions to the process reading the
frame.

As it happens, there is just one flag that's applicable only when reading
frames: `TUN_PKT_STRIP`.  This flag is set when the packet you just read is
too big for the buffer you've supplied.

The `flags` at this time is not used when *writing* frames back to the `tun`
device: when writing frames, this should be set to `0x0000`.

### `proto`

This is the Ethertype ID of the data that follows.  It is in network-endian
format (i.e. big-endian), and a list of possible values can be seen in
`linux/if_ether.h`, or alternatively on
[Wikipedia](https://en.wikipedia.org/wiki/EtherType#Examples).

Notable ones that likely matter here:

* `0x0800` (`ETH_P_IP`): Internet Protocol version 4
* `0x86dd` (`ETH_P_IPv6`): Internet Protocol version 6

### Decoding the actual packet data

This will depend on the protocol that your frame encapsulates, the raw data
for your frame will follow the 4 byte header you just read.  Inspect the
`proto` part of the header, then use that to choose an appropriate packet
dissection algorithm.

This might be done by copying data to a `struct iphdr` (`linux/ip.h`) if it's
an IPv4 payload or `struct ipv6hdr` (`linux/ipv6.h`) if it's IPv6.
Alternatively you might choose to implement the dissector using your own data
structures rather than using the Linux kernel ones.
