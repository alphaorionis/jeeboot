/// JeeBoot - Over-the-air RFM12B self-updater for ATmega (startup code)
// 2012-11-01 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include <avr/power.h>

#define bit(b) (1 << (b))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1 << (bit)))
#define bitClear(value, bit) ((value) &= ~(1 << (bit)))

typedef uint8_t byte;
typedef uint16_t word;

#include "ota_boot.h"

int main(void) __attribute__ ((naked)) __attribute__ ((section (".init9")));

volatile char dummy;

EMPTY_INTERRUPT(WDT_vect);

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

  // The Heart of the Matter. The Real Enchilada. The Meaning of Life.
  byte backoff = 0;
  while (run() > 100) {
    // the boot re-flashing failed for some reason, although the boot server
    // did respond, so do an exponential back-off with the clock speed reduced
    // (not as low-power as power down, but doesn't need watchdog interrupts)
    if (++backoff > 10)
      backoff = 0; // limit the backoff, reset to retry quickly after a while
    // here we go: slow down, waste some processor cyles, and speed up again
    // this has a total cycle time of a few hours, as determined empirically
    // (using a boot server which deliberately replies with a bad remote ID)
    clock_prescale_set(clock_div_256);
    for (long i = 0; i < 10000L << backoff && !dummy; ++i)
      ;
    clock_prescale_set(clock_div_4);
  }

  // force a clean reset to launch the actual code
  clock_prescale_set(clock_div_1);
  wdt_enable(WDTO_15MS);
  for (;;)
    ;
}
