JEEBOOT - Arduino over-the-air bootloader
=========================================

This bootloader allows JeeNodes to be reprogrammed (reflashed) over an RF12B radio.
For this purpose, the bootloader contains a small RF12B library and runs a very simple
but highly reliable boot protocol with a boot server. The outline of the boot protocol
is as follows:

- The first phase establishes the node's identity, called pairing: the node uses a standard radio
config and sends its hardware ID. This phase is intended to be run "on the bench" when setting up a node for a specific mission. The code can later be upgraded over the air and only needs to be re-paired if its identity and radio settings need to change.

If the hardware ID sent in the pairing request is zero the server responds with a random
hardware ID to write into the node and the node tries again with this hardware ID.
Then the boot server responds with the desired radio config (group_id and node_id).
At the end of this phase, the node is paired with a network, meaning it has a group ID and a node
ID baked into its non-volatile memory. It also has a unique hardware ID.

- The second phase establishes which software the node should be runinng and is used to upgrade
the node.
The node sends its hardware ID and the boot server responds with a software ID and a CRC checksum.
The node can then look at what is has in its flash and determine whether the software is OK and
ready to run, or whether new software needs to be uploaded.

- The third phase downloads the software.
If the software in the flash isn't right, the node downloads the right one one packet at
a time. At the end, it verifies the checksum and starts the sketch if all looks right, else
it goes back to phase 2 and tries again.

Boot logic
----------

What happens at boot, i.e., after the AVR is reset, is pretty tricky to ensure that nodes are
recoverable without a manual reset switch under almost all circumstances. The logic goes
as follows:

1. if the reset is due to power-on reset or brown-out and the loaded sketch is valid, then run it
2. if the reset is due to WDT and the loaded sketch is valid and we have pattern A in RAM,
then run the sketch
3. if the reste is due to an external reset or
(the reset is due to WDT and we have pattern B in RAM),
then invalide the current sketch (i.e. this forces an upgrade)
4. if the reset is due to WDT and we have no special pattern in RAM,
then perform the upgrade check once (takes just 250ms) and run the sketch if it's valid
5. perform the pairing at least once until we have a hardware ID, group id, and node id
6. perform the upgrade check until we have a response
7. download the sketch and go back into the upgrade check if the result is invalid (periodically go back into the pairing check)
8. it's all good, start the sketch
