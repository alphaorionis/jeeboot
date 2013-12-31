JeeNet Protocols
================

This page summarizes the packet formats and protocols used by the JeeLab networking
over a variety of transports. All the formats are pattered after the original JeeLib
RF12B packets format, except that instead of the CTL/DST/ACK flags a packet type is used
where appropriate. The packet types and their RF12B flag settings are:

```
Type        Purpose                                             CTL DST ACK NODE
data_push   Normal data packet, no ACK requested                 0   1   0  dest
data_req    Normal data packet, ACK requested                    0   1   1  dest
bcast_push  Broadcast data packet, no ACK requested              0   0   0   src
bcast_req   Broadcast data packet, ACK requested                 0   0   1   src
ack_data    ACK reply packet for a data packet                   1   0   0   src
ack_bcast   ACK reply packet for a broadcast packet              1   1   0  dest
pairing     Pairing request (reply is a boot_reply)              1   1   1     0
boot_req    Boot protocol request                                1   0   1   src
boot_reply  Boot protocol reply                                  1   1   1  dest
```

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
 - the UDP payload consists of:
   - a type byte (need to encode the table)
   - a node id byte
   - the packet data
   - a packet CRC-16 that is calculated over the entire UDP payload

There is no registration defined in V1: the router and the clients must know a-priori whom
to send what where.

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
 - a terminating newline

A different line-start character could be used to pass RF69 format packets...

HTTP
----

Each packet is transmitted using a POST request where the query string is used to encode
header information and the packet data payload is in the POST body.
 - `type=<type encoding>` : the packet type from the table (ex: `type=data_req`)
 - `node=<node id>` : source/dest node id
 - the data payload is in the POST body with the content-type set of application/binary
   and the content-length set to the payload length

There is no registration defined in V1: the router and the clients must know a-priori whom
to send what where.

WS (web sockets)
----------------

The websocket connection is opened by the client (connecting to the hub router) and the query
string of the connecting HTTP request contains the ID fo the source. The format of the query
string is: `group=<group_id>&node=<node_id>`.

Each packet is transmitted using a web sockets message. The encoding of the message is identical to the UDP packet encoding but without the CRC-16 at the end.
