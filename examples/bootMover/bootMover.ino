/// @dir bootMover
/// Prepare high-mem bootstrap code by moving it into place on an ATtiny.
// 2012-11-16 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include <avr/power.h>

// when built for ATmega, this code will run as "simulator" with debug output
#if defined(__AVR_ATtiny84__)
#define DEBUG 0
#else
#define DEBUG 1
#undef SPM_PAGESIZE
#define SPM_PAGESIZE 64
#endif

// the following include file was genereated by this command, using JeeMon:
//   ./hex2c.tcl ../../bootloader/ota_boot_attiny84.hex >data.h

#include "data.h"

byte progBuf [SPM_PAGESIZE];

// see http://www.nongnu.org/avr-libc/user-manual/group__avr__boot.html
static void boot_program_page (uint32_t page, byte *buf) {
#if DEBUG
  Serial.print(page, HEX);
  Serial.print(':');
  for (byte i = 0; i < 3; ++i) {
    Serial.print(' ');
    Serial.print(progBuf[i]);
  }
  Serial.print(" ...");
  for (byte i = SPM_PAGESIZE - 3; i < SPM_PAGESIZE; ++i) {
    Serial.print(' ');
    Serial.print(progBuf[i]);
  }
  Serial.println();
#endif

  boot_page_erase_safe(page);

  for (word i = 0; i < SPM_PAGESIZE; i += 2) {
    word w = *buf++;
    w += (*buf++) << 8;
    boot_page_fill_safe(page + i, w);
  }

  boot_page_write_safe(page);   // Store buffer in flash page.
  boot_spm_busy_wait();         // Wait until the memory is written.
}

void setup () {
#if DEBUG
  Serial.begin(57600);
  Serial.print("\n[bootMover] ");
  Serial.println(SPM_PAGESIZE);
#else
  cli();
#endif

  word block = sections[0].start / SPM_PAGESIZE;
  for (word offset = 0; offset < sections[0].count; offset += SPM_PAGESIZE) {
    memcpy_P(progBuf, progdata + offset, SPM_PAGESIZE);
    boot_program_page(block++ * SPM_PAGESIZE / 2, progBuf);
  }

  // patch the reset vector to jump to the boot loader
  memcpy_P(progBuf, 0, SPM_PAGESIZE);
  word* rvec = (word*) progBuf;
  *rvec &= ~0x0FFF;
  *rvec += sections[0].start >> 1;
  boot_program_page(0, progBuf);
}

void loop () {}
