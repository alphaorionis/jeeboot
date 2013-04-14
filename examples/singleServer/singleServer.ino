/// @dir singleServer
/// Serve a single sketch out of the Flash Board's on-board EEPROM memory.
/// Derived from isp_flash, captures code and allows re-flashing later.
// Based on original code by David A. Mellis and Randall Bohn.
// -jcw, 2013-03-25

// static char *id="@(#) $Id: ab.c,v 1.5 1993/10/19 14:57:32 ceder Exp $";
// man what

#include <JeeLib.h>
#include <JeeBoot.h>
#include <util/crc16.h>
  
#define MEGA 1    // 0 for ATtiny programming
#define DEBUG 2

// pin definitions
#define PIN_SCK   14  // AIO1
#define PIN_MISO  4   // DIO1
#define PIN_MOSI  17  // AIO4
#define PIN_RESET 7   // DIO4
#define LED_PMODE 15  // AIO2 - on while programming (LED to VCC via resistor)
#define START_BTN 5   // DIO2 - active low, starts programming target

// STK Definitions
#define STK_OK      '\x10'
#define STK_FAILED  '\x11'
#define STK_UNKNOWN '\x12'
#define STK_INSYNC  '\x14'
#define STK_NOSYNC  '\x15'
#define CRC_EOP     '\x20' //ok it is a space...

word here;          // word address for reading and writing, set by 'U' command
byte pageBuf[256];  // global block storage

// access to the 128 kbyte on-board EEPROM memory on port 3
PortI2C i2cBus (3);
MemoryPlug mem (i2cBus);

BootReply bootReply;
DataReply dataReply;

struct {
  byte devicecode;
  byte revision;
  byte progtype;
  byte parmode;
  byte polling;
  byte selftimed;
  byte lockbytes;
  byte fusebytes;
  int flashpoll;
  int eeprompoll;
  int pagesize;
  // not used in this code:
  // int eepromsize;
  // long flashsize;
  word blocks;    // number of 64-byte blocks to upload
  word crcTotal;  // the CRC over all those blocks
} param;

const char sigBytes[] = {
#if MEGA
  0x1E, 0x95, 0x0F // atmega328p
#else
  0x1E, 0x93, 0x0C // attiny84
#endif
};

static byte getch() {
  for (word i = 0; i < 50000; ++i)
    if (Serial.available())
      break;
  return Serial.read();
}

static word getWord () {
  byte b = getch();
  return (b << 8) | getch();
}
static void putch(char c) {
  Serial.print(c);
}

static void readbytes(int n) {
  for (byte x = 0; x < n; x++)
    pageBuf[x] = getch();
}

static void calcTotalCRC () {
  word blocks = (here + 31) / 32;
  word crc = ~0;
  for (word pos = 0; pos < blocks * 64; pos += param.pagesize) {
    mem.load(pos / param.pagesize, 0, pageBuf, param.pagesize);
    for (word i = 0; i < param.pagesize && pos + i < blocks * 64; ++i)
      crc = _crc16_update(crc, pageBuf[i]);
  }
  param.blocks = blocks;
  param.crcTotal = crc;
  mem.save(257, 0, &param, sizeof param);
}

static void spi_init() {
  digitalWrite(PIN_SCK, 1);
  digitalWrite(PIN_MISO, 1);
  digitalWrite(PIN_MOSI, 1);
  digitalWrite(PIN_RESET, 1);

  pinMode(PIN_SCK, OUTPUT);
  pinMode(PIN_MISO, INPUT);
  pinMode(PIN_MOSI, OUTPUT);
  pinMode(PIN_RESET, OUTPUT);
}

static byte spi_send(byte b) {
  byte reply = 0;
  for (byte i = 0; i < 8; ++i) {
    digitalWrite(PIN_MOSI, b & 0x80);
    digitalWrite(PIN_SCK, 0); // slow pulse, max 60KHz
    digitalWrite(PIN_SCK, 1);
    b <<= 1;
    reply = (reply << 1) | digitalRead(PIN_MISO);
  }
  return reply;
}

static byte spi_transaction(byte a, byte b, byte c, byte d) {
  spi_send(a); 
  spi_send(b);
  spi_send(c);
  return spi_send(d);
}

static byte spi_transaction_wait(byte a, byte b, byte c, byte d) {
  byte reply = spi_transaction(a, b, c, d);
  while (spi_transaction(0xF0, 0, 0, 0) & 1)
    ;
  return reply;
}

static void empty_reply() {
  if (getch() == CRC_EOP) {
    putch(STK_INSYNC);
    putch(STK_OK);
  } else
    putch(STK_NOSYNC);
}

static void breply(byte b) {
  if (getch() == CRC_EOP) {
    putch(STK_INSYNC);
    putch(b);
    putch(STK_OK);
  } else
    putch(STK_NOSYNC);
}

static void get_version(byte c) {
  switch(c) {
    case 0x80:  breply(2); break;
    case 0x81:  breply(1); break;
    case 0x82:  breply(18); break;
    case 0x93:  breply('S'); break;
    default:  breply(0);
  }
}

static void set_parameters() {
  // call this after reading parameter packet into pageBuf[]
  memcpy(&param, pageBuf, 9);
  // following fields are big endian
  param.eeprompoll = pageBuf[10] * 0x0100 + pageBuf[11];
  param.pagesize = pageBuf[12] * 0x0100 + pageBuf[13];
  // not used in this code:
  // param.eepromsize = pageBuf[14] * 0x0100 + pageBuf[15];
  // param.flashsize = pageBuf[16] * 0x01000000L + pageBuf[17] * 0x00010000L +
  //             pageBuf[18] * 0x00000100 + pageBuf[19];
}

static void start_pmode() {
  spi_init();
  // following delays may not work on all targets...
  pinMode(PIN_RESET, OUTPUT);
  digitalWrite(PIN_RESET, HIGH);
  pinMode(PIN_SCK, OUTPUT);
  digitalWrite(PIN_SCK, LOW);
  delay(50);
  digitalWrite(PIN_RESET, LOW);
  delay(50);
  pinMode(PIN_MISO, INPUT);
  pinMode(PIN_MOSI, OUTPUT);
  spi_transaction_wait(0xAC, 0x53, 0x00, 0x00);
  digitalWrite(LED_PMODE, 0); 
}

static void end_pmode() {
  pinMode(PIN_MISO, INPUT);
  pinMode(PIN_MOSI, INPUT);
  pinMode(PIN_SCK, INPUT);
  pinMode(PIN_RESET, INPUT);
  digitalWrite(PIN_RESET, HIGH);
  digitalWrite(LED_PMODE, 1); 
}

static void flash(byte hilo, int addr, byte value) {
  spi_transaction_wait(0x40+8*hilo, addr >> 8, addr, value);
}

static void write_flash (word addr) {
  mem.load(addr / (param.pagesize >> 1), 0, pageBuf, param.pagesize);
  for (word x = 0; x < param.pagesize; x += 2) {
    flash(LOW, addr, pageBuf[x]);
    flash(HIGH, addr, pageBuf[x+1]);
  }
  spi_transaction_wait(0x4C, addr >> 8, addr, 0);
}

static void program_page() {
  char result = STK_FAILED;
  word length = getWord();
  if (length <= sizeof pageBuf) {
    memset(pageBuf, 0xFF, param.pagesize);
    char memtype = getch();
    readbytes(length);
    if (getch() == CRC_EOP) {
      putch(STK_INSYNC);
      if (memtype == 'F') {
        mem.save(here / (param.pagesize >> 1), 0, pageBuf, param.pagesize);
        result = STK_OK;
      }
    } else
      result = STK_NOSYNC;
  }
  putch(result);
}

static void read_page() {
  char result = STK_FAILED;
  word length = getWord();
  char memtype = getch();
  if (getch() == CRC_EOP && length <= sizeof pageBuf) {
    putch(STK_INSYNC);
    if (memtype == 'F') {
      mem.load(here / (param.pagesize >> 1), 0, pageBuf, length);
      for (int x = 0; x < length; x += 2) {
        putch(pageBuf[x]);
        putch(pageBuf[x+1]);
        here++;
      }
      result = STK_OK;
    }
  } else
    result = STK_NOSYNC;
  putch(result);
}

static void read_signature() {
  if (getch() == CRC_EOP) {
    putch(STK_INSYNC);
    // putch(spi_transaction(0x30, 0x00, 0x00, 0x00));
    // putch(spi_transaction(0x30, 0x00, 0x01, 0x00));
    // putch(spi_transaction(0x30, 0x00, 0x02, 0x00));
    putch(sigBytes[0]);
    putch(sigBytes[1]);
    putch(sigBytes[2]);
    putch(STK_OK);
  } else
    putch(STK_NOSYNC);
}

static byte fake_spi (byte a, byte b, byte c, byte d) {
  // Read Lock bits          $58 $00 $00 data byte out
  // Read Signature Byte     $30 $00 0000 000aa data byte out
  // Read Fuse bits          $50 $00 $00 data byte out
  // Read Fuse High bits     $58 $08 $00 data byte out
  // Read Extended Fuse Bits $50 $08 $00 data byte out
  switch (a) {
    case 0x58:
      if (b == 0x00)
        return 0xFF; // lock bits
      else
        return 0xFF; // fuse high
    case 0x50:
      return 0xFF; // fuse low
    case 0x30: // signature
      return sigBytes[c];
  }
}

static int avrisp() { 
  switch (getch()) {
    case '0': // signon
      empty_reply();
      break;
    case '1':
      if (getch() == CRC_EOP) {
        putch(STK_INSYNC);
        for (const char* p = "AVR ISP"; *p != 0; ++p)
          putch(*p);
        putch(STK_OK);
      }
      break;
    case 'A':
      get_version(getch());
      break;
    case 'B':
      readbytes(20);
      set_parameters();
      empty_reply();
      break;
    case 'E': // extended parameters - ignore for now
      readbytes(5);
      empty_reply();
      break;
    case 'P':
      // start_pmode();
      empty_reply();
      break;
    case 'U':
      here = getch();
      here += 256 * getch();
      empty_reply();
      break;
    case '`': //STK_PROG_FLASH
      getch();
      getch();
      empty_reply();
      break;
    case 'a': //STK_PROG_DATA
      getch();
      empty_reply();
      break;
    case 'd': //STK_PROG_PAGE
      program_page();
      break;
    case 't': //STK_READ_PAGE
      read_page();  
      break;
    case 'V':
      readbytes(4);
      breply(fake_spi(pageBuf[0], pageBuf[1], pageBuf[2], pageBuf[3]));
      break;
    case 'Q':
      // end_pmode();
      empty_reply();
      calcTotalCRC();
      break;
    case 'u': //STK_READ_SIGN
      read_signature();
      break;
      // expecting a command, not CRC_EOP
      // this is how we can get back in sync
    case CRC_EOP:
      putch(STK_NOSYNC);
      break;
      // anything else we will return STK_UNKNOWN
    default:
      putch(getch() == CRC_EOP ? STK_UNKNOWN : STK_NOSYNC);
  }
}

static void programmer () {
  Serial.print("\nprogram: ");
  start_pmode();

  Serial.print(here);
  Serial.print(' ');
  Serial.print(param.pagesize);
  Serial.print(' ');
  
  // spi_transaction_wait(0xAC, 0xE0, 0x00, 0xFF); // unlock
  spi_transaction_wait(0xAC, 0x80, 0x00, 0x00); // chip erase
  
  for (word addr = 0; addr < here; addr += param.pagesize >> 1) {
    Serial.print('.');
    write_flash(addr);
  }

  // print signature to check the device and make sure we're still in sync
  Serial.print(" <");
  Serial.print(spi_transaction(0x30, 0x00, 0x00, 0x00), HEX);
  Serial.print(',');
  Serial.print(spi_transaction(0x30, 0x00, 0x01, 0x00), HEX);
  Serial.print(',');
  Serial.print(spi_transaction(0x30, 0x00, 0x02, 0x00), HEX);
  Serial.println("> done");
  
  end_pmode();
  
  // make sure the button is no longer pressed
  while (digitalRead(START_BTN) == 0)
    ;
  delay(100);
}

static void dumpBytes (const void* ptr, word len) {
  const char* data = (const char*) ptr;
  for (word i = 0; i < len; ++i) {
    if (i && i % 16 == 0)
      Serial.println();
    Serial.print(' ');
    byte b = data[i];
    if (b < 16)
      Serial.print('0');
    Serial.print(b, HEX);
  }
  Serial.println();
}

static void sendReply (const void* ptr, byte len) {
  // Serial.flush(); // FIXME needed to work around a bug (!?)
  rf12_sendNow(RF12_ACK_REPLY, ptr, len);
  rf12_sendWait(2);
}

static void initialRequest (word rid) {
  bootReply.remoteID = 0;
  bootReply.sketchBlocks = param.blocks;
  bootReply.sketchCRC = param.crcTotal;
      
  sendReply(&bootReply, sizeof bootReply);
    
#if DEBUG 
  Serial.print("ir ");
  Serial.print(rid);
  Serial.print(" sb ");
  Serial.print(bootReply.sketchBlocks);
  Serial.print(" crc ");
  Serial.print(bootReply.sketchCRC);
  Serial.print(" ps ");
  Serial.println(param.pagesize);
  Serial.println(" -> ack ");
#if DEBUG > 2
  Serial.print("  send:");
  for (byte i = 0; i < sizeof bootReply; ++i) {
    if (i % 16 == 0)
      Serial.println();
    Serial.print(' ');
    byte b = ((byte*) &bootReply)[i];
    if (b < 16)
      Serial.print('0');
    Serial.print(b, HEX);
  }
  Serial.println();
#endif
  Serial.flush();
#endif
}

static void dataRequest (word rid, word blk) {
  dataReply.info = blk ^ rid;
  word pos = 64 * blk;
    
  mem.load(pos / param.pagesize, pos % param.pagesize, dataReply.data, 64);
  sendReply(&dataReply, sizeof dataReply);
       
#if DEBUG 
  Serial.print("dr ");
  Serial.print(rid);
  Serial.print(" # ");
  Serial.print(blk);
#if DEBUG > 1
  Serial.print(" -> ");
  Serial.print(dataReply.info);
  Serial.print(" @ ");
  Serial.print(pos);
#endif
#if DEBUG > 2
  Serial.print("  send:");
  for (byte i = 0; i < 64; ++i) {
      Serial.print(' ');
      Serial.print(pageBuf[i], HEX);
  }
#endif
  Serial.println();
  Serial.flush();
#endif
}

void setup() {
#if MEGA
  Serial.begin(57600);
#else
  Serial.begin(9600);
#endif
#if DEBUG 
  Serial.println("\n[singleServer]");
#endif
  pinMode(LED_PMODE, OUTPUT);
  pinMode(START_BTN, INPUT);
  digitalWrite(START_BTN, 1);
  digitalWrite(LED_PMODE, 1);

  rf12_initialize(1, RF12_868MHZ, 254);

  mem.load(257, 0, &param, sizeof param);
  here = param.blocks * 32;

#if DEBUG 
  Serial.print("h ");
  Serial.print(here);
  Serial.print(" ps ");
  Serial.print(param.pagesize);
  Serial.print(" crc ");
  Serial.println(param.crcTotal);
#if DEBUG > 1
  mem.load(0, 0, pageBuf, param.pagesize);
  dumpBytes(pageBuf, param.pagesize);
#endif
  Serial.flush();
#endif
}

void loop(void) {
  if (Serial.available())
    avrisp();

  if (digitalRead(START_BTN) == 0) {
#if DEBUG
    Serial.begin(57600);
#endif
    programmer();
  }

  if (rf12_recvDone() && rf12_crc == 0 && RF12_WANTS_ACK) {
    digitalWrite(LED_PMODE, 0); 
    const word* args = (const word*) rf12_data;
    if (rf12_len == 2)
      initialRequest(args[0]);
    else if (rf12_len == 4)
      dataRequest(args[0], args[1]);
    digitalWrite(LED_PMODE, 1); 
  }
}
