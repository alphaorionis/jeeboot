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

#define DEBUG 1

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

static uint32_t millis () {
  return 0; // FIXME not correct
}

static void sleep (word ms) {
  // ...
}

#if DEBUG
#include <stdio.h>
static int putch(char, FILE *stream);
static inline void flash_led(uint8_t);
static FILE serout = FDEV_SETUP_STREAM(putch, NULL, _FDEV_SETUP_WRITE);
#define dump(...)
#else
#define putch(...)
#define printf(...)
#define flash_led(...)
#define dump(...)
#endif

#include "ota_RF12.h"
#include "boot.h"

#ifdef DEBUG
#define BAUD_RATE 57600L
#define UART 0
#if UART == 0
# define UART_SRA UCSR0A
# define UART_SRB UCSR0B
# define UART_SRC UCSR0C
# define UART_SRL UBRR0L
# define UART_UDR UDR0
#else
#error UART == 1, but no UART1 on device
#endif

#define LED_DDR     DDRD
#define LED_PORT    PORTD
#define LED_PIN     PIND
#define LED         PIND4
#endif /* DEBUG */

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

#ifdef DEBUG
  // init UART
  UART_SRA = _BV(U2X0); //Double speed mode USART0
  UART_SRB = _BV(RXEN0) | _BV(TXEN0);
  UART_SRC = _BV(UCSZ00) | _BV(UCSZ01);
  UART_SRL = (uint8_t)( (4000000L + BAUD_RATE * 4L) / (BAUD_RATE * 8L) - 1 );
  // Set up Timer 1 for timeout counter
  TCCR1B = _BV(CS12) | _BV(CS10); // div 1024
  // Set LED pin as output
  LED_DDR |= _BV(LED);
  stdout = &serout;
#endif

  // similar to Adaboot no-wait mod
  if (!launch) {
    flash_led(12); // 6 flashes
		printf("Reset!\n");
    clock_prescale_set(clock_div_1);
    ((void(*)()) 0)(); // Jump to RST vector
  }

  // switch to 4 MHz, the minimum rate needed to use the RFM12B
  clock_prescale_set(clock_div_4);

  flash_led(4); // 2 flashes
	printf("Cold start!\n");

  bootLoader();

  // force a clean reset to launch the actual code
  clock_prescale_set(clock_div_1);
  wdt_enable(WDTO_15MS);
  for (;;)
    ;
}

#ifdef DEBUG

static int putch(char ch, FILE *stream) {
  while (!(UART_SRA & _BV(UDRE0)));
  UART_UDR = ch;
	return 0;
}

void watchdogReset();
void flash_led(uint8_t count) {
  do {
    TCNT1 = -(F_CPU/(1024*16));
    TIFR1 = _BV(TOV1);
    while(!(TIFR1 & _BV(TOV1)));
    LED_PIN |= _BV(LED);
    watchdogReset();
  } while (--count);
}
// Watchdog functions. These are only safe with interrupts turned off.
void watchdogReset() {
  __asm__ __volatile__ (
    "wdr\n"
  );
}
#endif
