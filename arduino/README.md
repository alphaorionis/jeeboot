The `testServer2` sketch turns a JeeNode or JeeLink into a test boot server
for the new JeeBoot design. It will send out upgrade requests for three blink
apps, blinking in red, green, or blue on EA's LPC MAX or NXP's LPC812 board.
These blink sketches were built using arm-gcc, turned into a static C array
using the hex2c.tcl utility (needs Tclkit or JeeMon), and then hard-wired into
the test server. Each new boot request will cycle through these blink apps.

Just copy the testServer2 folder into the Arduino IDE's "sketchbook location".
