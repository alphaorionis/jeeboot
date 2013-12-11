#include "LPC8xx.h"
#include "uart_int.h"
#include <string.h>

#define UART_ENABLE          (1 << 0)
#define UART_DATA_LENGTH_8   (1 << 2)
#define UART_PARITY_NONE     (0 << 4)
#define UART_STOP_BIT_1      (0 << 6)

/* Status bits */
#define UART_STATUS_RXRDY    (1 << 0)
#define UART_STATUS_RXIDLE   (1 << 1)
#define UART_STATUS_TXRDY    (1 << 2)
#define UART_STATUS_TXIDLE   (1 << 3)
#define UART_STATUS_CTSDEL   (1 << 5)
#define UART_STATUS_RXBRKDEL (1 << 11)

#define UART_INTEN_RXRDY     (1 << 0)
#define UART_INTEN_TXRDY     (1 << 2)

template <int SIZE>
class RingBuffer {
  uint8_t buffer[SIZE];
  volatile uint8_t fill, take;
public:
  RingBuffer () : fill (SIZE), take (SIZE) {}
  bool isEmpty () const { return fill == take; }
  bool isFull () const { return (fill + 1) % SIZE == take; }
  void add (uint8_t c) { buffer[fill] = c; fill = (fill + 1) % SIZE; }
  uint8_t pop () { uint8_t c = buffer[take]; take = (take + 1) % SIZE; return c; }
};

RingBuffer<100> rxBuf;
RingBuffer<100> txBuf;

void uart0Init (uint32_t baudRate) {
  uint32_t clk;
  const uint32_t UARTCLKDIV=1;

  /* Setup the clock and reset UART0 */
  LPC_SYSCON->UARTCLKDIV = UARTCLKDIV;
  // NVIC_DisableIRQ(UART0_IRQn);
  LPC_SYSCON->SYSAHBCLKCTRL |=  (1 << 14);
  LPC_SYSCON->PRESETCTRL    &= ~(1 << 3);
  LPC_SYSCON->PRESETCTRL    |=  (1 << 3);

  /* Configure UART0 */
  clk = __SYSTEM_CLOCK / UARTCLKDIV;
  LPC_USART0->CFG = UART_DATA_LENGTH_8 | UART_PARITY_NONE | UART_STOP_BIT_1;
  LPC_USART0->BRG = clk / 16 / baudRate - 1;
  LPC_SYSCON->UARTFRGDIV = 0xFF;
  LPC_SYSCON->UARTFRGMULT = (((clk / 16) * (LPC_SYSCON->UARTFRGDIV + 1)) /
    (baudRate * (LPC_USART0->BRG + 1))) - (LPC_SYSCON->UARTFRGDIV + 1);

  /* Clear the status bits */
  LPC_USART0->STAT = UART_STATUS_CTSDEL | UART_STATUS_RXBRKDEL;

  /* Enable UART0 interrupt */
  NVIC_EnableIRQ(UART0_IRQn);

  /* Enable UART0 */
  LPC_USART0->INTENSET = UART_INTEN_RXRDY;
  LPC_USART0->CFG |= UART_ENABLE;
}

void uart0SendChar (char buffer) {
  while (txBuf.isFull())
    ;
  LPC_USART0->INTENCLR = UART_INTEN_TXRDY;
  txBuf.add(buffer);
  LPC_USART0->INTENSET = UART_INTEN_TXRDY;
}

void uart0Send (const char *buffer, uint32_t length) {
  while (length--)
    uart0SendChar(*buffer++);
}

int uart0RecvChar () {
  LPC_USART0->INTENCLR = UART_INTEN_RXRDY;
  int result = -1;
  if (!rxBuf.isEmpty())
    result = rxBuf.pop();
  LPC_USART0->INTENSET = UART_INTEN_RXRDY;
  return result;
}

extern "C" void UART0_IRQHandler () {
  uint32_t stat = LPC_USART0->STAT;
  if (stat & UART_STATUS_RXRDY)
    rxBuf.add(LPC_USART0->RXDATA);
  if (stat & UART_STATUS_TXRDY) {
    if (txBuf.isEmpty())
      LPC_USART0->INTENCLR = UART_INTEN_TXRDY;
    else
      LPC_USART0->TXDATA = txBuf.pop();
  }
}
