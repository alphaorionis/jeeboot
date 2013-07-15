/// @dir pairServer
/// Initial test of a possible pairing protocol and mechanism - server side.
// -jcw, 2013-07-15

#include <JeeLib.h>
#include <JeeBoot.h>
#include <avr/eeprom.h>

#define EEPROM_ADDR  ((byte*) 0x000)
#define EEPROM_MAGIC 123

word lastVal;
PairReply pairings [32];

static void print2hex (byte value) {
  Serial.print(value >> 4, HEX);
  Serial.print(value & 0x0F, HEX);
}

static void printWidth (word value, byte width) {
  if (width > 4 && value <= 9999) Serial.print(' ');
  if (width > 3 && value <= 999) Serial.print(' ');
  if (width > 2 && value <= 99) Serial.print(' ');
  if (width > 1 && value <= 9) Serial.print(' ');
  Serial.print(value);
  Serial.print(' ');
}

static void listPairings () {
  Serial.println(F("  ID GROUP  TYPE SHARED KEY"));
  Serial.println(F("  -- ----- ----- --------------------------------"));
  byte n = 0;
  for (byte i = 1; i < 32; ++i) {
    PairReply* pr = pairings + i;
    if (pr->nodeId) {
      printWidth(pr->nodeId, 4);
      printWidth(pr->group, 5);
      printWidth(pr->type, 5);
      for (byte j = 0; j < 16; ++j)
        print2hex(pr->sharedKey[j]);
      Serial.println();
      ++n;
    }
  }
  Serial.print(n, DEC);
  Serial.print(F(" pairings, defaults: group "));
  Serial.print(pairings[0].group, DEC);
  Serial.print(F(", type "));
  Serial.println(pairings[0].type, DEC);
}

static void setRandomKey (byte* key) {
  do
    for (byte i = 0; i < 16; ++i)
      key[i] = random(0, 255);
  while (key[0] == 0); // make sure 1st byte isn't zero
}

static void writeFullEeprom () {
  Serial.print(F("writing... "));
  eeprom_write_block(pairings, EEPROM_ADDR, sizeof pairings);
  Serial.println(F("done"));
}

static void writePartialEeprom (char id) {
  // this is fast enough that no progress message is needed
  const byte size = sizeof pairings[0];
  eeprom_write_block(&pairings[id], EEPROM_ADDR + id * size, size);
}

static void erasePairings () {
  memset(pairings, 0, sizeof pairings);
  pairings[0].nodeId = EEPROM_MAGIC;
  pairings[0].group = 212;
  pairings[0].type = 1;
  writeFullEeprom();
}

static void readEeprom () {
  eeprom_read_block(pairings, EEPROM_ADDR, sizeof pairings);
  if (pairings[0].nodeId == EEPROM_MAGIC)
    Serial.println(F("pairings reloaded"));
  else
    erasePairings();
}

static byte assignSlot () {
  byte id;
  for (id = 1; id < 30; ++id)
    if (pairings[id].sharedKey[0] == 0)
      break;

  PairReply* pr = pairings + id;
  pr->version = 1;
  pr->nodeId = id;
  if (pr->group == 0)
    pr->group = pairings[0].group;
  if (pr->type == 0)
    pr->type = pairings[0].type;
  setRandomKey(pr->sharedKey);

  // save right away (but only save this one pairing, since it's a bit slow)
  writePartialEeprom(id);
  return id;
}

static void printHelp () {
  Serial.println(F("Available commands:"));
  Serial.println(F("  l           - list current pairings"));
  Serial.println(F("  <grp> g     - set default group (1..250) [212]"));
  Serial.println(F("  <type> t    - set default type (0..65535) [1]"));
  Serial.println(F("  <id> f      - forget pairing for given node (1..30)"));
  Serial.println(F("  123 e       - erase all pairings"));
  // not shown but still present, current code automatically manages eeprom:
//Serial.println(F("  r           - reload pairings from EEPROM"));
//Serial.println(F("  w           - write pairings to EEPROM"));
}

static void processKey (int ch) {
  if ('0' <= ch && ch <= '9')
    lastVal = 10 * lastVal + ch - '0';
  else if (ch >= 0) {
    switch (ch) {
      case 'l': listPairings(); break;
      case 'g': pairings[0].group = lastVal; writePartialEeprom(0); break;
      case 't': pairings[0].type = lastVal; writePartialEeprom(0); break;
      case 'f': memset(&pairings[lastVal], 0, sizeof pairings[0]); break;
      case 'e': if (lastVal == 123) erasePairings(); break;
      case 'r': readEeprom(); break;
      case 'w': writeFullEeprom(); break;
      case ' ': return; // don't clear lastVal
      default:
        if (ch > ' ')
          printHelp();
    }
    lastVal = 0;
  }
}

void setup () {
  Serial.begin(57600);
  Serial.println("\n[pairServer]");
  rf12_initialize(31, RF12_868MHZ, 212);
  printHelp();
  readEeprom();
}

void loop () {
  processKey(Serial.read());

  if (rf12_recvDone() && rf12_crc == 0 && rf12_hdr == RF12_HDR_DST &&
      rf12_len >= sizeof (PairRequest)) {

    randomSeed(millis()); // randomise based on packet reception time
    byte id = assignSlot();
    rf12_sendNow(0, &pairings[id], sizeof pairings[id]);

    Serial.print(F("paired: id "));
    Serial.print(id, DEC);
    Serial.print(F(", group "));
    Serial.print(pairings[id].group, DEC);
    Serial.print(F(", type "));
    Serial.println(pairings[id].type);
  }
}
