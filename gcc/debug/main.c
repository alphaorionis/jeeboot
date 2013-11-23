#include "LPC8xx.h"
#include <stdio.h>
#include "uart.h"

volatile uint32_t msTicks;

void SysTick_Handler (void) {
  ++msTicks;
}

static void delay_ms (uint32_t ms) {
  uint32_t now = msTicks;
  while ((msTicks-now) < ms)
    ;
}

static void configurePins (void) {
  /* Enable clocks to IOCON & SWM */
  LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 18) | (1 << 7);
  /* Pin Assign 8 bit Configuration */
  /* U0_TXD */
  /* U0_RXD */
  LPC_SWM->PINASSIGN0 = 0xffff0106UL; 
}

int main (void) {
  configurePins();
  uart0Init(9600);
  
  SysTick_Config(__SYSTEM_CLOCK/1000);   // 1000 Hz
  LPC_GPIO_PORT->DIR0 |= (1 << 7);

  printf("clock %d\r\n", __SYSTEM_CLOCK);
    
  while (1) {
    LPC_GPIO_PORT->NOT0 = 1 << 7;
    delay_ms(500);
  }
  
  return 0;
}