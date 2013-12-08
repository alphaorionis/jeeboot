// JeeBoot server, code in dataflash, configuration via serial connection.

// -jcw, 2013-12-07

#include "LPC8xx.h"
#include <stdio.h>

extern "C" {
#include "uart.h"
#include "rf69_12.h"
#include "dataflash.h"
}

const int redLed = 17;
const int greenLed = 16;
const int blueLed = 7;

volatile uint32_t msTicks;

class MilliTimer {
    uint32_t lastTime;
  public:
    MilliTimer () : lastTime (msTicks) {}
    
    bool poll (unsigned ms) {
      uint32_t now = msTicks;
      if ((now - lastTime) < ms)
        return false;
      lastTime = now; // TODO: not accurate when done this way!
      return true;
    }
};

extern "C" void SysTick_Handler (void) {
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
#if LPC_BES
  /* disable SWD */
  LPC_SWM->PINENABLE0 |= (1 << 2) | (1 << 3);
  /* U0_TXD */
  /* U0_RXD */
  LPC_SWM->PINASSIGN0 = 0xffff0004UL;
  /* SPI0_SCK 12 */
  LPC_SWM->PINASSIGN3 = 0x0cffffffUL; 
  /* SPI0_MOSI 14 */
  /* SPI0_MISO 15 */
  /* SPI0_SSEL 13 */
  LPC_SWM->PINASSIGN4 = 0xff0d0f0eUL;
#endif
}

int main (void) {
  configurePins();
  uart0Init(57600);
  printf("\n[server]\n");
  
  SysTick_Config(__SYSTEM_CLOCK/1000);   // 1000 Hz
  LPC_GPIO_PORT->DIR0 |= (1 << redLed) | (1 << greenLed) | (1 << blueLed);
  LPC_GPIO_PORT->SET0 = (1 << redLed) | (1 << greenLed) | (1 << blueLed);

  int dfId = df_init();
  printf("dfId 0x%04X\n", dfId);
  
#if 0
  LPC_GPIO_PORT->NOT0 = (1 << greenLed);
  df_eraseEntireChip();
  while (df_busy())
    ;
  LPC_GPIO_PORT->NOT0 = (1 << greenLed);
#endif
  
  // printf("clock %u\n", __SYSTEM_CLOCK);
  rf12_initialize(31, RF12_868MHZ, 5);

  MilliTimer blinkTimer;
  int interval = 999;
    
  while (true) {
    // generate a very brief green LED blip once a second
    if (blinkTimer.poll(interval)) {
      LPC_GPIO_PORT->NOT0 = (1 << greenLed);
      interval = 1000 - interval;
    }
    
    if (rf12_recvDone() && rf12_crc == 0) {
      // briefly turn red LED on while reporting incoming package over serial
      LPC_GPIO_PORT->CLR0 = (1 << redLed);
      printf("OK %d", rf12_hdr);
      for (int i = 0; i < rf12_len; ++i)
        printf(" %d", rf12_data[i]);
      printf("\n");
      LPC_GPIO_PORT->SET0 = (1 << redLed);
    }
  }
  
  return 0;
}
