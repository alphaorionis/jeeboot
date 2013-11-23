#define LPC_MAX 1

#define __VTOR_PRESENT 1

#include "LPC8xx.h"
#include <stdio.h>
#include "uart.h"
#include "spi.h"
#include "rf69_12.h"
#include "iap_driver.h"

// #define printf(...)
#define dump(...)

static volatile uint32_t msTicks;

void SysTick_Handler (void) {
  ++msTicks;
}

static void delay_ms (uint32_t ms) {
  // TODO: enter low-power sleep mode
  uint32_t now = msTicks;
  while ((msTicks - now) < ms)
    ;
  // TODO: exit low-power sleep mode
}

extern uint16_t _crc16_update (uint16_t crc, uint8_t data);

#include "boot.h"

static void configurePins (void) {
  /* Enable clocks to IOCON & SWM */
  LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 18) | (1 << 7);
  /* Pin Assign 8 bit Configuration */
#ifndef printf
  /* U0_TXD */
  /* U0_RXD */
  LPC_SWM->PINASSIGN0 = 0xffff0106UL; 
#endif
#if LPC_MAX
  // irq 8 ?
  /* Pin Assign 8 bit Configuration */
  /* SPI0_SCK 12 */
  LPC_SWM->PINASSIGN3 = 0x0cffffffUL; 
  /* SPI0_MOSI 14 */
  /* SPI0_MISO 15 */
  /* SPI0_SSEL 13 */
  LPC_SWM->PINASSIGN4 = 0xff0d0f0eUL;
#else
  /* Pin Assign 8 bit Configuration */
  /* SPI0_SCK 14 */
  LPC_SWM->PINASSIGN3 = 0x0effffffUL; 
  /* SPI0_MOSI 13 */
  /* SPI0_MISO 12 */
  /* SPI0_SSEL 10 */
  LPC_SWM->PINASSIGN4 = 0xff0a0c0dUL;
#endif
}

static void launchApp() {
  printf("launchApp\n");
  SCB->VTOR = (uint32_t) BASE_ADDR;
  // __asm("LDR SP, [R0]    ;Load new stack pointer address")
  void (*fun)() = (void (*)()) ((uint32_t*) BASE_ADDR)[1];
  printf("go!\n");
  fun();
  printf("launchApp failed\n");
}

int main (void) {
  configurePins();
#ifndef printf
  uart0Init(57600);
#endif
  
  SysTick_Config(__SYSTEM_CLOCK/1000);   // 1000 Hz

  printf("clock %lu\n", __SYSTEM_CLOCK);
  // int e = iap_init();
  // printf("iap init %d\n", e);
  uint32_t partId;
  iap_read_part_id(&partId);
  printf("part id 0x%04X\n", (int) partId);
  
  rf12_initialize(1, RF12_868MHZ, 212);

  // this will not catch the runaway case when the server replies with data,
  // but the application that ends up in memory does not match the crc given
  // in this case, we'll constantly keep retrying... and drain the battery :(
  // to avoid this, an extra level of exponential back-off has been added here
  for (int backOff = 0; /*forever*/; ++backOff) {
    bootLoaderLogic();
    if (appIsValid())
      break;
    delay_ms(100 << (backOff & 0x0F));
  }

  launchApp();
  
  return 0;
}
