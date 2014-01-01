GOHUB ROUTER
============

The GoHub router is a JeeNet hub router written in Go (http://golang.org).
It communicates with RF gateways over UDP and application servers over
HTTP. It's #1 role in life is to route packets mainly between embedded
nodes (e.g. JeeNodes) and applications/services that do something with
the data or manage the nodes.

The router listens on a UDP port and an HTTP port and expects others to
send it packets through those ports. It then examines the packets and
applies a set of routing rules that determine how the router forwards
the packets. It may forward a packet to multiple destinations, to a single
destination, it may log the packet, or it may drop it on the floor.
The UDP and HTTP packet formats are described in <../Network.md>.

Routing
-------

The routing is very simple and operates on the idea that each peer connecting
to the router registers for a set of packets that it wants to receive. Initially
this registration is static and captured in a JSON config file, but the intent is
that ultimetaly peers will be able to self-register when they connect to the
router.

The registration associates a peer, identified by name, with a connection
URL and a set of packet matchers.  The currently recognized URLs are
`udp://ip_address:port` and `http://hostname:port/url/prefix`.

The set of packet matchers are:
 - match a source peer from which the packet was received by name
 - match a header field (type, group, node, length) value
 - match a payload byte (indexed) value

An important routing rule is that a peer's matchers will never match a
packet that the peer sent to the router, i.e., the router will never loop
the packet back to its source.

### Sample config

```
{ peers = {
  # Define networks of nodes and associate with group id so all packets with that
  # group ID gets sent to the network's gateway
  house_nodes = {                      # a set of JeeNodes around the house
    url = "udp://192.168.0.24:9999",   # the RF12-UDP gateway
    match = [
      { group = 0xD4 }                 # the group_id of those nodes
    ],
  },
  garden_nodes = {                     # a set of JeeNodes in the garden
    url = "udp://garden-gw:9999",      # the RF12-UDP gateway
    match = [
      { group = 0xD5 }                 # the group_id of those nodes
    ],
  },
  # Applications
  logger = {
    url = "log://localhost",           # special built-in peer
    match = [],                        # the empty list matches all packets
  },
  heating = {                          # the heating system
    url = "http://localhost:8010/",
    match = [
      { group = 0xD4 },                # all nodes for this are in that network
      { index = 0,                     # match first data byte
        range = [ 10,19 ],             #   having values 10 through 19
      },
    ],
  }
  greenhouse = {                       # greenhouse control
    url = http://localhost:8011/",
    match = [
      { group = 0xD5 },                # all nodes for this are in that network
      { index = 0,                     # match first data byte
        range = [ 0,32 ],              #   having values 0 through 32
      },
    ],
  },
}, }
```


Routing (abandoned attempt)
-------

The routing uses the following concepts:
 - Peers are directly connected entities to which the router can send a
   packet, these may be rf-UDP gateways, applications, etc. Each peer has
   a name and a method for forwarding a packet to it (for example an IP
   address and UDP port).
 - Packet fields are named or indexed bytes in a packet. Named fields
   are used to refer to header fields (type, group, node, length) and
   indexes are used to refer to bytes in the payload.

The router operates according to a set of peer definitions and routing
rules that are specified in a route.js file in JSON format. The intent
is that in the future peers can register with the router and ask to
receive the

### Routing rules

Each routing rule matches a number of packet properties and specifies a list of actions.
The set of match functions is:
 - match a source peer from which the packet was received
 - match a field value in the packet
 - logical AND of a number of matches

The set of actions is:
 - forward the packet to a named peer
 - log the packet
 - set the type, group, and/or node field of the packet
 - restart routing (useful after modifying the packet)

### 
