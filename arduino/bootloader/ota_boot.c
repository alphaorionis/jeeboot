/// JeeBoot - Over-the-air RFM12B self-updater for ATmega (startup code)
// 2012-11-01 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <util/crc16.h>

#define bit(b) (1 << (b))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1 << (bit)))
#define bitClear(value, bit) ((value) &= ~(1 << (bit)))

typedef uint8_t byte;
typedef uint16_t word;

#define ARDUINO 1
#define printf(...)
#define dump(...)

#define REMOTE_TYPE 0x100
#define PAIRING_GROUP 212

uint32_t hwId [4];  

static uint32_t millis () {
  return 0; // FIXME not correct
}

static void sleep (word ms) {
  // ...
}

#include "ota_RF12.h"
#include "boot.h"

/* The main function is in init9, which removes the interrupt vector table */
/* we don't need. It is also 'naked', which means the compiler does not    */
/* generate any entry or exit code itself. */
int main(void) __attribute__ ((naked)) __attribute__ ((section (".init9")));

int main () {
  // cli();
  // SP=RAMEND;  // This is done by hardware reset
  asm volatile ("clr __zero_reg__");

  // find out whether we got here through a watchdog reset
  byte launch = bitRead(MCUSR, EXTRF);
  MCUSR = 0;
  wdt_disable();

  // similar to Adaboot no-wait mod
  if (!launch) {
    clock_prescale_set(clock_div_1);
    ((void(*)()) 0)(); // Jump to RST vector
  }

  // switch to 4 MHz, the minimum rate needed to use the RFM12B
  clock_prescale_set(clock_div_4);

  bootLoader();

  // force a clean reset to launch the actual code
  clock_prescale_set(clock_div_1);
  wdt_enable(WDTO_15MS);
  for (;;)
    ;
}
