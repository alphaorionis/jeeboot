/// JeeBoot - Over-the-air RFM12B self-updater for ATmega (startup code)
// 2012-11-01 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php

#include <inttypes.h>
#include <string.h>
#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "boot.h"
#include <avr/power.h>
#include <avr/wdt.h>
#include <util/crc16.h>

//===== CONFIGURATION =====

// Enable debug functions (LED blinking and/or serial output)
// 0->none, 1->LED Port1-D, 2->serial 57600kbps
#define DEBUG 2
// Set the board type and board version
#define REMOTE_TYPE 0x100  // "board=1 version=0"
// Check upgrade on watchdog reset
#define WDT_UPGRADE 0
// RF group to use for pairing protocol
#define PAIRING_GROUP 212

//===== END OF CONFIGURATION =====

#define bit(b) (1 << (b))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1 << (bit)))
#define bitClear(value, bit) ((value) &= ~(1 << (bit)))

typedef uint8_t byte;
typedef uint16_t word;

#define ARDUINO 1

/* Timer 1 used for network time-out and for blinking LEDs */
static void timer_init() {
  TCCR1B = _BV(CS12) | _BV(CS10);            // div 1024 -- @4Mhz=3906Hz
}
static void timer_start(int16_t millis) {
  TCNT1 = -(4000L * (int32_t)millis / 1024); // 4000=4Mhz/1000, 1024=clk divider
  TIFR1 = _BV(TOV1);                         // clear overflow flag
}

static uint8_t timer_done() {
  return TIFR1 & _BV(TOV1);
}

// TODO: LOW POWER! This needs to power down when sleeping!
static void sleep(uint32_t ms) {
  while(ms > 1000) {
    timer_start(1000);
    while (!timer_done())
      ;
    ms -= 1000;
  }
  if (ms > 0) {
    timer_start(ms);
    while (!timer_done())
      ;
  }
}

#include "debug.h"
#include "ota_RF12.h"
#include "loader.h"

//===== BOOT PATTERN =====

#define PATTADDR 0x100

// save the pattern at boot time
static uint32_t pattern __attribute__ ((section (".noinit")));
void save_pattern(void) __attribute__ ((naked)) __attribute__ ((section (".init3")));
void save_pattern(void) {
  pattern = *(uint32_t*)(PATTADDR+0);
}

// pattern A says: reboot immediately on WDT reset
static byte isPatternA() {
#if 0
  P("\nP:"); P_X16(pattern&0xffff); P_X16(pattern>>16);
#endif
  return pattern == 0xb00dbeef;
}

static void setPatternA() {
  *(uint32_t *)PATTADDR = 0xb00dbeef;
#if 0
  P("\np:");
  P_X16(*(uint16_t*)(PATTADDR+0));
  P_X16(*(uint16_t*)(PATTADDR+2));
#endif
}

// pattern B says: invalidate the sketch on WDT reset
static byte isPatternB() {
  return pattern == 0x0badf00d;
}

//===== MAIN =====

/* The main function is in init9, which removes the interrupt vector table */
/* we don't need. It is also 'naked', which means the compiler does not    */
/* generate any entry or exit code itself. */
int main(void) __attribute__ ((OS_main)) __attribute__ ((section (".init9")));

int main () {
  // cli();
  asm volatile ("clr __zero_reg__");

  // read reset cause and clear it, make sure WDT is disabled
  byte cause = MCUSR;
  MCUSR = 0;
  wdt_disable();

  timer_init();

#if DEBUG & 2
  // init UART
  UART_SRA = _BV(U2X0); //Double speed mode USART0
  UART_SRB = _BV(RXEN0) | _BV(TXEN0);
  UART_SRC = _BV(UCSZ00) | _BV(UCSZ01);
  UART_SRL = (uint8_t)( (4000000L + BAUD_RATE * 4L) / (BAUD_RATE * 8L) - 1 );
#endif
#if DEBUG & 1
  // Set LED pin as output
  LED_DDR |= _BV(LED);
  LED_PIN |= _BV(LED); // necessary to turn off LED if LED is on when pin is low
#endif

#if 0
  clock_prescale_set(clock_div_4);
  P("\nc");
  P_X8(cause);
  P(" pA ");
  P(isPatternA()?"Y":"N");
  P_LN;
#endif

  // check whether the sketch is valid
  loadConfig();
  byte valid = appIsValid();

  // if we got power-on reset or brown-out reset and the sketch is valid, run it
  // if we got a wdt reset and the sketchy is valid and we have pattern A in RAM then run sketch
  // this is similar to Adaboot no-wait mod
  if ((valid && (cause & (1<<BORF) || cause & (1<<PORF)))
  ||  (valid && (cause & (1<<WDRF) && isPatternA())))
  {
    flash_led(2); // 1 flash
    clock_prescale_set(clock_div_1);
    *(byte *)0x100 = config.group;
    *(byte *)0x101 = config.nodeId;
    ((void(*)()) 0)(); // Jump to RST vector
  }

  byte quick = 0;
  // we're gonna zero the target software ID to force an upgrade if we have an external reset
  // or a WDT reset with pattern B in RAM
  if (cause & (1<<EXTRF) || ( (cause & (1<<WDRF) && isPatternB()) )) {
    appInvalidate();
  // we do a single upgrade check and then run the sketch if we have a WDT reset and no RAM pattern
  } else if (valid && (cause & (1<<WDRF))) {
    quick = 1;
  }

  // switch to 4 MHz, the minimum rate needed to use the RFM12B
  clock_prescale_set(clock_div_4);

  flash_led(4); // 2 flashes
  P("\n\nBOOT!\n");

  bootLoader(quick);

  // force a clean reset to launch the actual code
  P("APP\n");
  flash_led(6); // 3 flashes
  clock_prescale_set(clock_div_1);
  setPatternA();
  wdt_enable(WDTO_15MS);
  for (;;)
    ;
}
