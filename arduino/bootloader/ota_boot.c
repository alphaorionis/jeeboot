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

// undef->none, 1->LED Port1-D, 2->serial 57600kbps
#define DEBUG 3

#define bit(b) (1 << (b))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1 << (bit)))
#define bitClear(value, bit) ((value) &= ~(1 << (bit)))

typedef uint8_t byte;
typedef uint16_t word;

#define ARDUINO 1

#define REMOTE_TYPE 0x100
#define PAIRING_GROUP 212

uint32_t hwId [4];  

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

// TODO: LOW POWER!
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

/* The main function is in init9, which removes the interrupt vector table */
/* we don't need. It is also 'naked', which means the compiler does not    */
/* generate any entry or exit code itself. */
int main(void) __attribute__ ((OS_main)) __attribute__ ((section (".init9")));

int main () {
  // cli();
  asm volatile ("clr __zero_reg__");

  // find out whether we got here through a watchdog reset
  byte launch = bitRead(MCUSR, EXTRF);
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
#endif

  // similar to Adaboot no-wait mod
  if (!launch) {
    flash_led(2); // 1 flash
    clock_prescale_set(clock_div_1);
    ((void(*)()) 0)(); // Jump to RST vector
  }

  // switch to 4 MHz, the minimum rate needed to use the RFM12B
  clock_prescale_set(clock_div_4);

  flash_led(4); // 2 flashes
	P("\n\nBOOT!\n");

  bootLoader();

  // force a clean reset to launch the actual code
	P("APP\n");
	flash_led(6); // 3 flashes
  clock_prescale_set(clock_div_1);
  wdt_enable(WDTO_15MS);
  for (;;)
    ;
}
