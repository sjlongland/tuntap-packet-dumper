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
* `0x86dd` (`ETH_P_IPV6`): Internet Protocol version 6

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

If you cast the address of the `data` member of `struct tundev_frame_t` to
a `struct ethhdr*`, you can extract the Ethernet frame protocol from that
struct's `h_proto` member (again, big-endian) and the source/destination MAC
addresses via `h_source` and `h_dest` respectively.

`h_proto` is once again, an Ethertype ID.  Since this is layer 2, you will
see other protocols here in addition to IPv4 and IPv6 mentioned above, e.g.

* `0x0806` (`ETH_P_ARP`): Address Resolution Protocol
* `0x8100` (`ETH_P_8021Q`): IEEE 802.1Q VLAN

# Example output

## With packet information (`-no-pi` omitted)

### Raw IPv4 datagram

```
Flags: 0x0000  Proto: 0x0800
EType: 0x0800
To:    ff:ff:ff:ff:ff:ff
From:  ce:32:0b:4c:93:48
  98:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 45 00 00 54 00 00 40 00 40 01 e1 78 ac 18 00 01
  16: ac 18 00 ff 08 00 ee b6 d5 62 00 23 82 25 85 66
  32: 00 00 00 00 60 64 0d 00 00 00 00 00 10 11 12 13
  48: 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f 20 21 22 23
  64: 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f 30 31 32 33
  80: 34 35 36 37 01 30 01 30 01 30 01 30 01 30 01 30
  96: 01 30
```

### Raw ARP

```
Flags: 0x0000  Proto: 0x0806
EType: 0x0806
To:    ff:ff:ff:ff:ff:ff
From:  ce:32:0b:4c:93:48
  42:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 00 01 08 00 06 04 00 01 ce 32 0b 4c 93 48 ac 18
  16: 00 01 00 00 00 00 00 00 ac 18 00 02 00 00 00 00
  32: 00 00 00 00 00 00 00 02 85 00
```

### Raw IPv6 datagram

```
Flags: 0x0000  Proto: 0x86dd
EType: 0x86dd
To:    33:33:00:00:00:01
From:  ce:32:0b:4c:93:48
IP Version 6  Priority: 0
Flow Label:  0x0c19bf
Payload length: 64
Next header: 0x3a
Hop limit:   1
Source IP: fe80::cc32:bff:fe4c:9348
Dest IP:   ff02::1
 118:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 60 0c 19 bf 00 40 3a 01 fe 80 00 00 00 00 00 00
  16: cc 32 0b ff fe 4c 93 48 ff 02 00 00 00 00 00 00
  32: 00 00 00 00 00 00 00 01 80 00 03 13 d3 be 00 02
  48: 96 23 85 66 00 00 00 00 63 08 04 00 00 00 00 00
  64: 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f
  80: 20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f
  96: 30 31 32 33 34 35 36 37 03 69 70 36 04 61 72 70
 112: 61 00 00 0c 80 01
```

### IPv4 within 802.1Q

```
Flags: 0x0000  Proto: 0x0800
EType: 0x8100
To:    ff:ff:ff:ff:ff:ff
From:  ce:32:0b:4c:93:48
 102:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 40 7b 08 00 45 00 00 54 00 00 40 00 40 01 e1 76
  16: ac 19 00 01 ac 19 00 ff 08 00 0b 08 d5 80 00 14
  32: b7 25 85 66 00 00 00 00 19 04 03 00 00 00 00 00
  48: 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f
  64: 20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f
  80: 30 31 32 33 34 35 36 37 01 30 01 30 01 30 01 30
  96: 01 30 01 30 01 30
```

### ARP within 802.1Q

```
Flags: 0x0000  Proto: 0x0806
EType: 0x8100
To:    ff:ff:ff:ff:ff:ff
From:  ce:32:0b:4c:93:48
  46:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 40 7b 08 06 00 01 08 00 06 04 00 01 ce 32 0b 4c
  16: 93 48 ac 19 00 01 00 00 00 00 00 00 ac 19 00 02
  32: d3 25 85 66 00 00 00 00 61 b6 08 00 00 00
```

### IPv6 within 802.1Q

```
Flags: 0x0000  Proto: 0x86dd
EType: 0x8100
To:    33:33:00:00:00:01
From:  ce:32:0b:4c:93:48
802.1Q VLAN 123: Priority: 2 DEI: N Datagram protocol: 0x86dd
IP Version 6  Priority: 0
Flow Label:  0x0c19bf
Payload length: 64
Next header: 0x3a
Hop limit:   1
Source IP: fe80::cc32:bff:fe4c:9348
Dest IP:   ff02::1
 122:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 40 7b 86 dd 60 0c 19 bf 00 40 3a 01 fe 80 00 00
  16: 00 00 00 00 cc 32 0b ff fe 4c 93 48 ff 02 00 00
  32: 00 00 00 00 00 00 00 00 00 00 00 01 80 00 12 a1
  48: 0d 16 00 09 e9 3d 85 66 00 00 00 00 c8 01 03 00
  64: 00 00 00 00 10 11 12 13 14 15 16 17 18 19 1a 1b
  80: 1c 1d 1e 1f 20 21 22 23 24 25 26 27 28 29 2a 2b
  96: 2c 2d 2e 2f 30 31 32 33 34 35 36 37 6b 69 73 68
 112: 69 c0 23 00 1c 80 01 00 00 00
```

## Without packet information (`-no-pi` provided)

### Raw ARP datagram

```
EType: 0x0806
To:    ff:ff:ff:ff:ff:ff
From:  ce:32:0b:4c:93:48
  42:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 00 01 08 00 06 04 00 01 ce 32 0b 4c 93 48 ac 18
  16: 00 01 00 00 00 00 00 00 ac 18 00 02 00 00 00 00
  32: 00 00 00 00 00 00 00 02 85 00
```

### Raw IPv4 datagram

```
EType: 0x0800
To:    ff:ff:ff:ff:ff:ff
From:  ce:32:0b:4c:93:48
  98:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 45 00 00 54 00 00 40 00 40 01 e1 78 ac 18 00 01
  16: ac 18 00 ff 08 00 3f ef d6 a8 00 03 f6 26 85 66
  32: 00 00 00 00 98 04 0f 00 00 00 00 00 10 11 12 13
  48: 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f 20 21 22 23
  64: 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f 30 31 32 33
  80: 34 35 36 37 01 30 01 30 01 30 01 30 01 30 01 30
  96: 01 30
```

### Raw IPv6 datagram

```
EType: 0x8100
To:    33:33:00:00:00:01
From:  ce:32:0b:4c:93:48
802.1Q VLAN 123 Priority 2 DEI N
IP Version 6  Priority: 0
Flow Label:  0x0c19bf
Payload length: 64
Next header: 0x3a
Hop limit:   1
Source IP: fe80::cc32:bff:fe4c:9348
Dest IP:   ff02::1
 122:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 40 7b 86 dd 60 0c 19 bf 00 40 3a 01 fe 80 00 00
  16: 00 00 00 00 cc 32 0b ff fe 4c 93 48 ff 02 00 00
  32: 00 00 00 00 00 00 00 00 00 00 00 01 80 00 06 52
  48: 0b d4 00 03 00 3c 85 66 00 00 00 00 b3 9a 0e 00
  64: 00 00 00 00 10 11 12 13 14 15 16 17 18 19 1a 1b
  80: 1c 1d 1e 1f 20 21 22 23 24 25 26 27 28 29 2a 2b
  96: 2c 2d 2e 2f 30 31 32 33 34 35 36 37 6b 69 73 68
 112: 69 c0 23 00 1c 80 01 00 00 00
```

### 802.1q carrying ARP

```
EType: 0x8100
To:    ff:ff:ff:ff:ff:ff
From:  ce:32:0b:4c:93:48
  46:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 40 7b 08 06 00 01 08 00 06 04 00 01 ce 32 0b 4c
  16: 93 48 ac 19 00 01 00 00 00 00 00 00 ac 19 00 02
  32: 00 00 00 00 00 00 00 02 85 00 a5 9f 00 00
```

### 802.1q carrying IPv4

```
EType: 0x8100
To:    ff:ff:ff:ff:ff:ff
From:  ce:32:0b:4c:93:48
 102:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
   0: 40 7b 08 00 45 00 00 54 00 00 40 00 40 01 e1 76
  16: ac 19 00 01 ac 19 00 ff 08 00 3a 9b d7 15 00 04
  32: 54 27 85 66 00 00 00 00 45 ea 08 00 00 00 00 00
  48: 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f
  64: 20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f
  80: 30 31 32 33 34 35 36 37 01 30 01 30 01 30 01 30
  96: 01 30 01 30 01 30
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
