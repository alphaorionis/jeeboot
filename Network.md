JeeNet Protocols
================

This page summarises the packet formats and protocols used by the JeeLab networking
over a variety of transports.

RF12B
-----
The packet format is binary and is sent using the RF12B radio. Each packet consists of:
 - Hardware SYN byte: 0x??
 - Network group byte (doubles as 2nd SYN byte): 0xD4 default, but can be changed by the user
 - Header byte: `<CTL, DST, ACK, node_id>`
 - Length byte: length of data payload (i.e. the next field)
 - Data payload: 0..66 bytes
 - CRC: 16-bit CRC

The A bit (ACK) indicates whether this packet wants to get an ACK back.
The C bit needs to be zero in this case (the name is somewhat confusing).

The D bit (DST) indicates whether the node ID specifies the destination
node or the source node. For packets sent to a specific node, DST =
1. For broadcasts, DST = 0, in which case the node ID refers to the
originating node.

The C bit (CTL) is used to send ACKs, and in turn must be combined with the A bit set to zero.

To summarize, the following combinations are used:
 - normal packet, no ACK requested: CTL = 0, ACK = 0
 - normal packet, wants ACK: CTL = 0, ACK = 1
 - ACK reply packet: CTL = 1, ACK = 0
 - the CTL = 1, ACK = 1 combination is not currently used

RF69
----
The RF69 packet format is almost identical to the RF12B one with a few transposed fields and
a smaller maximum payload length.
 - Hardware SYN byte: 0x??
 - Network group byte (doubles as 2nd SYN byte): 0xD4 default, but can be changed by the user
 - Length byte: length of header byte + data payload
 - Header byte: `<CTL, DST, ACK, node_id>`
 - Data payload: 0..63 bytes
 - CRC: 16-bit CRC

UDP
---

The packet format is binary and is sent via UDP. Each packet consists of:
 - standard UDP header with src/dst IPs and src/dst ports
 - the UDP payload consists of the RF12B network group, header, length and data payload fields
 - (should we use the CRC too?)

Note that this encodes the length twice: implicitly in the UDP packet length and explicitly
in the length field.

Questions:
 - Should we add some integrity check, such as the crc-16
 - Should we take the packet apart and have byte fields for source and dest
   (setting them to 0 or -1 if unknown) and a separate header/flags byte?

SERIAL 1
--------

(jcw has a serial protocol in jeeboot that I can't quite figure out)

SERIAL 2
--------

(This is from a serial bridge sketch I wrote a while ago)
The packet format is base64 encoded binary. Each packet is encoded as a newline-terminated line.
 - each line starts with '!', lines without '!' are ignored
 - a length character encoding the number of base64 4-character groups to follow as 'A'+N (`N = (data_bytes+2)/3`)
 - rf12b packet from the group byte through the crc, all base64 encoded
 - terminating newline

HTTP
----

Each packet is transmitted using a POST request where the query string is used to encode
header information and the packet data payload is in the POST body.
 - `hdr=CDA`: the 3 header flags from the rf12b packet where presence of a charcater
   indicates that the bit is set, e.g., for a rf12b header of 0xC3 the query string
   representation is `hdr=CD`
 - `group=<group id>` : group ID in decimal (212 is JeeNode default)
 - `source=<node id>` : source node id, if known
 - `dest=<node id>` : dest node id, if known
 - the data payload is in the POST body with the content-type set of application/binary
   and the content-length set to the payload length

Questions:
  - Should we take the header flags apart?
