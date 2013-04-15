// JeeBoot - Over-the-air RFM12B self-updater for ATmega (boot loader code)
// 2012-11-01 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php

#include <avr/boot.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <util/crc16.h>
#include "ota_RF12.h"

#define BOOT_ARCH   1             // diff for each boot loader setup (1..30)
#define BOOT_REV    0             // boot loader revision (0..7)
#define BOOT_FREQ   RF12_868MHZ   // frequency band used to contact boot server
#define BOOT_GROUP  254           // boot server's net group >= BOOT_BASE
#define BOOT_BASE   248

#define EEADDR ((byte*) (1024 - 4 - sizeof config))

struct Config {
  byte revision :3;       // increment this to invalidate the EEPROM contents
  byte srvFreq :2;        // BOOT_FREQ
  byte srvGroup :3;       // BOOT_GROUP - BOOT_BASE
  byte spare [3];
  word remoteID;          // these must be same as struct BootReply
  word sketchBlocks;      // ... with same three items
  word sketchCRC;         // ... and in same order
  byte psk [16];
  word crc;               // must be last
} config;

struct BootReply {
  word remoteID;          // these must be the same as in struct Config
  word sketchBlocks;      // ... with the same three items
  word sketchCRC;         // ... and in the same order
};

struct DataRequest {
  word remoteID;
  word block;
} dreq;

byte progBuf [SPM_PAGESIZE];

#if TESTING
#define T(x) x
#else
#define T(x)
#endif

static word calcCRC (const void* ptr, word len) {
  word crc = ~0;
  for (word i = 0; i < len; ++i)
    crc = _crc16_update(crc, ((const char*) ptr)[i]);
  return crc;
}

static word calcCRCrom (const void* ptr, word len) {
    word crc = ~0;
    for (word i = 0; i < len; ++i)
        crc = _crc16_update(crc, pgm_read_byte((word) ptr + i));
    return crc;
}

static byte validSketch () {
  return calcCRCrom(0, config.sketchBlocks << 6) == config.sketchCRC;
}

// see http://www.nongnu.org/avr-libc/user-manual/group__avr__boot.html
static void boot_program_page (uint32_t page, byte *buf) {
  // byte sreg = SREG;
  // cli();

  eeprom_busy_wait ();

  boot_page_erase (page);
  boot_spm_busy_wait ();      // Wait until the memory is erased.

  for (word i = 0; i < SPM_PAGESIZE; i += 2) {
    word w = *buf++;
    w += (*buf++) << 8;
    boot_page_fill (page + i, w);
  }

  boot_page_write (page);     // Store buffer in flash page.
  boot_spm_busy_wait();       // Wait until the memory is written.

#ifdef RWWSRE
  // Reenable RWW-section again. We need this if we want to jump back
  // to the application after bootloading.
  boot_rww_enable ();
#endif

  // Re-enable interrupts (if they were ever enabled).
  // SREG = sreg;
}

// timeouts = number of 200 ms periods before timing-out
static byte sendPacket (const void* buf, byte len, byte timeouts) {
  while (!rf12_canSend())
    rf12_recvDone();
  rf12_sendStart(RF12_HDR_ACK, buf, len);

  T(long t = millis());
  for (word m = 0; m < timeouts; ++m)
  {
    // this loop leads to a timeout of approx 200 ms without needing millis()
    for (word n = 0; n < 65000; ++n)
      if (rf12_recvDone() && rf12_crc == 0) {
        byte len = rf12_len;
        T(Serial.print("Got:"));
        for (byte i = 0; i < len; ++i) {
          T(if (i % 16 == 2) Serial.println());
          T(Serial.print(' '));
          T(Serial.print(rf12_data[i], DEC));
        }
        T(Serial.println());
        return len;
      }
  }
  T(Serial.print("timeout "));
  T(Serial.println(millis() - t));
  return 0;
}

static byte run () {
  // get EEPROM info, but use defaults if the stored CRC is not valid
  eeprom_read_block(&config, EEADDR, sizeof config);

  if (config.revision != BOOT_REV || calcCRC(&config, sizeof config) != 0) {
    memset(&config, 0, sizeof config);
    config.revision = BOOT_REV;
    config.srvFreq = BOOT_FREQ;
    config.srvGroup = BOOT_GROUP - BOOT_BASE;
  }

  rf12_initialize(BOOT_ARCH, config.srvFreq, config.srvGroup + BOOT_BASE);

  // send an update check to the boot server - just once, no retries
  byte bytes = sendPacket(&config.remoteID, sizeof config.remoteID, 10);
  if (bytes != sizeof (struct BootReply))
    return validSketch() ? 100 : 101; // unexpected reply length

  // the reply tells us which sketch version we should be running
  struct BootReply *reply = (struct BootReply*) rf12_data;
  if (reply->remoteID != config.remoteID)
    return 102; // this reply isn't for me

  // only reflash if desired version is different or current rom is invalid
  if (memcmp(&config.remoteID, reply, bytes) != 0 || !validSketch()) {

    // permanently save the desired sketch info in EEPROM
    memcpy(&config.remoteID, reply, bytes);
    config.crc = calcCRC(&config, sizeof config - 2);
    eeprom_write_block(&config, EEADDR, sizeof config);

    // start the re-flashing loop, asking for all the necessary data as ACKs
    dreq.remoteID = config.remoteID;

    for (dreq.block = 0; dreq.block < config.sketchBlocks; ++dreq.block) {
      // ask for the next block, retrying a few times
      byte attempts = 0;
      for (;;) {
        if (sendPacket(&dreq, sizeof dreq, attempts + 1) == 66) {
          word check = *((const word*) rf12_data);
          if (check == (dreq.remoteID ^ dreq.block))
            break;
        }
        if (++attempts == 10)
          return 103; // too many failed attempts to get the next data block
      }

      // save recv'd data, currently only works for a page size of 128 bytes    
      byte off = (dreq.block << 6) % SPM_PAGESIZE;
      if (off == 0)
          memset(progBuf, 0xFF, sizeof progBuf);
      memcpy(progBuf + off, (const byte*) rf12_data + 2, 64);
      if ((off == SPM_PAGESIZE - 64) || (dreq.block == config.sketchBlocks - 1))
        boot_program_page((dreq.block & ~1) << 6, progBuf);
    }
  }

  if (!validSketch())
    return 104; // the sketch doesn't qualify as a valid one

  return config.revision;
}
