# Simple packet dump utility

This just reads packets from a tun device and prints them out on the console.
It is known to work with Linux kernel 4.15.  The intent of this program is to
explore how packets can be read and dissected from the tap interface.

## TUN/TAP frame format (for `tun` devices)

The TUN/TAP interface basically spits out raw frames to you.  You have the
option of having the kernel pre-pend a "packet information" header to this.
It is useful when dealing with traffic that encapsulates IP datagrams, but can
muddy the waters when other encapsulation protocols (e.g. 802.1Q VLANs) are
thrown into the mix.

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

## `tap` differences

For `tap` devices, the format is the same.  You get the same packet
information header, and it'll even duplicate the "ethertype" field.  However
instead of an IP datagram, you'll get a raw Ethernet frame.  Thus, you'll need
to manage sending ARP requests (IPv4) or neighbour solicitation messages
(IPv6), and manage a MAC address table.

# Example output

## With `-no-pi`

### Raw IPv6 datagram

```
EType: 0x86dd
To:    33:33:00:00:00:01
From:  ce:32:0b:4c:93:48
 118:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 60 0c 19 bf 00 40 3a 01 fe 80 00 00 00 00 00 00
  16: cc 32 0b ff fe 4c 93 48 ff 02 00 00 00 00 00 00
  32: 00 00 00 00 00 00 00 01 80 00 ef 4a c1 a7 00 02
  48: 9b 03 85 66 00 00 00 00 85 07 03 00 00 00 00 00
  64: 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f
  80: 20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f
  96: 30 31 32 33 34 35 36 37 34 35 36 37 6b 69 73 68
 112: 69 c0 23 00 1c 80
```

### 802.1q carrying IPv6 traffic

VLAN is 123 (`0x7b`), priority 2.

```
EType: 0x8100
To:    33:33:00:00:00:01
From:  ce:32:0b:4c:93:48
 122:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 40 7b 86 dd 60 0c 19 bf 00 40 3a 01 fe 80 00 00
  16: 00 00 00 00 cc 32 0b ff fe 4c 93 48 ff 02 00 00
  32: 00 00 00 00 00 00 00 00 00 00 00 01 80 00 a3 f1
  48: c1 a1 00 02 8b 03 85 66 00 00 00 00 e2 66 01 00
  64: 00 00 00 00 10 11 12 13 14 15 16 17 18 19 1a 1b
  80: 1c 1d 1e 1f 20 21 22 23 24 25 26 27 28 29 2a 2b
  96: 2c 2d 2e 2f 30 31 32 33 34 35 36 37 6b 69 73 68
 112: 69 c0 23 00 1c 80 01 00 00 00
```
