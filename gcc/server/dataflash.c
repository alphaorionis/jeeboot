// Manage the DataFlash memory chip

#include "dataflash.h"
#include "LPC8xx.h"

// PIO0 pin assignment for the BESRAM board
const int CSEL = 2;
const int MISO = 3;
const int MOSI = 6;
const int SCLK = 7;

void enable () { LPC_GPIO_PORT->CLR0 = (1 << CSEL); }
void disable () { LPC_GPIO_PORT->SET0 = (1 << CSEL); }

static int xferByte (int value) {
  // TODO: usign bit-banging for now, should use 2nd SPI device
  int reply = 0;
  for (int i = 0; i < 8; ++i) {
    // TODO: there has to be a much better way to set an I/O pin...
    if (value & 0x80)
      LPC_GPIO_PORT->SET0 = (1 << MOSI);
    else
      LPC_GPIO_PORT->CLR0 = (1 << MOSI);
    LPC_GPIO_PORT->CLR0 = (1 << SCLK);
    LPC_GPIO_PORT->SET0 = (1 << SCLK);
    value <<= 1;
    reply = (reply << 1) | ((LPC_GPIO_PORT->PIN0 >> MISO) & 1);
  }
  return reply;
}

static void enableCmdAddr (int cmd, int addr) {
  enable();
  xferByte(cmd);
  xferByte(addr >> 16);
  xferByte(addr >> 8);
  xferByte(addr);
}

static void writeEnable () {
  enable();
  xferByte(0x06); // Write Enable
  disable();
}

int df_init () {
  disable();
  LPC_GPIO_PORT->DIR0 |= (1 << CSEL) | (1 << MOSI) | (1 << SCLK);
  LPC_GPIO_PORT->SET0 = (1 << SCLK);
  
  enable();
  xferByte(0x9F); // JEDEC ID
  int reply = xferByte(0) << 16;
  reply |= xferByte(0) << 8;
  reply |= xferByte(0);
  disable();
  
  return reply;
}

int df_isBusy () {
  enable();
  xferByte(0x05); // Read Status Register-1
  int busy = xferByte(0) & 1;
  disable();
  return busy;
}
  
void df_eraseEntireChip () {
  writeEnable();
  enable();
  xferByte(0xC7); // Chip Erase
  disable();
}
  
void df_eraseSector (int addr) {
  writeEnable();
  enableCmdAddr(0x20, addr); // Sector Erase
  disable();
}
  
void df_writeBytes (int addr, const void* buf, int len) {
  writeEnable();
  enableCmdAddr(0x02, addr); // Page Program
  for (int i = 0; i < len; ++i)
    xferByte(((const char*) buf)[i]);
  disable();
}
  
void df_readBytes (int addr, void* buf, int len) {
  enableCmdAddr(0x03, addr); // Page Program
  for (int i = 0; i < len; ++i)
    ((char*) buf)[i] = xferByte(0);
  disable();
}
