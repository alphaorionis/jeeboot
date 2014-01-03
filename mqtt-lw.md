MQTT-LW (lightweight) Protocol
==============================

This is an attempt at defining a lighter weight version of MQTT than MQTT-SN.

Packet format:
 - network/group id (1 byte)
 - length (approx 1 byte, but can't use 2 extra bits?)
 - destination node id (1 byte, node 0 = broadcast, 2 bits used for parity)
 - source node id (1 byte, 1 wants-ack bit, 1 bit unused?)
 - message type or quicktopic (N non-publish types, 256-N quicktopics)
 - payload (up to 61? bytes due to rfm69)
 - crc-16

Message types:
 - quickpub: short form of publish that uses the group id, node id and the type/topic byte
   to form an "automatic" topic 
 - ack: ACKs previous message
 - connect: inform the GW that I exist so it sets up topics & subscriptions (maybe we don't
   need that)
 - register: get 16-bit topic ID for a topic name (this could be used as implicit "register"?!)
 - regack: reply to register with topic ID
 - publish: generic publish with 16-bit topic ID
 - subscribe: generic subscribe with 16-bit topic ID
 - unsubscribe: generic unsubscription
 - boot: pairing and boot protocol (could use 3 types too)

Note that this means there are 8 explicit message types and 256-8=248
quickpub topics, we should reserve additional explicit types, I'm just
gonna roll with 8 for now

Link-level reliability:
 - If a node receives a packet with the wants-ack bit set it must immediately
   send an ACK message back before sending the other node any other message
 - If a node sends a message with the wants-ack bit set it may retransmit the
   message after Nms up to M times with randomized exponential back-off
 - Broadcast messages have no ACKs

Automatic topics and subscriptions:
 - quickpub message data is published in a topic named
   "/rfm12/<group_id>/<node_id>/<sub_topic>"
   where <xxx> are the unsigned decimal string representation of the binary field,
   and the sub_topic is the value of the message type/topic byte field
 - the QoS of quickpub and publish messages is determined with the wants-ack flag and is
   QoS=0 or QoS=1 respectively (oops, I just discovered that a subscription also has
   a QoS value, e.g., a publisher may use QoS=1 and a subscriber may subscribe with
   QoS=2, dunno what to make of that)
 - when nodes first send a message to the GW they are auto-subscribed to the topics
   :/rfm12/<group_id>/<node_id>/# (this assumes that nodes won't receive the messages
   they published, if that's not the case, we need to separate the quickpub topic tree
   from the auto-subscribed tree
 - nodes that are unheard of from the GW for 48 hours are treated as if they had
   disconnected (i.e. their subscriptions are removed)

Automatic retention:
 - quickpub topics #128-#248 automatically retain the last value
 - quickpub topics #0-#127 do not retail the last value
 - the last value sent to quickpub topic #0 is the will (do we want this feature?)

Questions:
 - Do we want gateway advertisement capability? I think not, just use node id for
   the gw by convention
 - It might be nice to have connect/disconnect capability so nodes can "sign off",
   but in the end it may just be easier to use a specific topic for that
 - We should simplify the publish message vs mqtt-sn: drop the flags, drop the
   msg-id: saves 3 bytes
 - We should simplify the subscribe message: drop mgs-id, maybe also flags (could
   use 3 message types to encode long/short/pre-def topic types)
 - Is ping useful?
 - There's stuff in MQTT-SN for nodes to tell the gateway that they're going to sleep
   for a while and that factors into node liveness state. Might be useful...?
