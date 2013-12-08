// Manage the DataFlash memory chip

#include "dataflash.h"
#include "LPC8xx.h"
#include <stdio.h>

// PIO0 pin assignment for the BESRAM board
const int CSEL = 2;
const int MISO = 3;
const int MOSI = 6;
const int SCLK = 7;

static void* memset(void* dst, uint8_t fill, int len) {
  uint8_t* to = (uint8_t*) dst;
  while (--len >= 0)
    *to++ = fill;
  return dst;
}

static void enable () {
  LPC_GPIO_PORT->B0[CSEL] = 0;
}

static void disable () {
  LPC_GPIO_PORT->B0[CSEL] = 1;
}

static int xferByte (int value) {
  // TODO: using bit-banging for now, could use 2nd SPI device
  int reply = 0;
  for (int i = 7; i >= 0; --i) {
    LPC_GPIO_PORT->B0[MOSI] = (value >> i) & 1;
    LPC_GPIO_PORT->B0[SCLK] = 0;
    LPC_GPIO_PORT->B0[SCLK] = 1;
    reply |= LPC_GPIO_PORT->B0[MISO] << i;
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
  while (df_isBusy())
    ;
  enable();
  xferByte(0x06); // Write Enable
  disable();
}

int df_init (void) {
  disable();
  LPC_GPIO_PORT->DIR0 |= (1 << CSEL) | (1 << MOSI) | (1 << SCLK);
  LPC_GPIO_PORT->B0[SCLK] = 1;
  
  enable();
  xferByte(0x9F); // JEDEC ID
  int reply = xferByte(0) << 16;
  reply |= xferByte(0) << 8;
  reply |= xferByte(0);
  disable();
  
  return reply;
}

int df_isBusy (void) {
  enable();
  xferByte(0x05); // Read Status Register-1
  int busy = xferByte(0) & 1;
  disable();
  return busy;
}

int df_isEmpty (int addr, int len) {
  enableCmdAddr(0x03, addr); // Read Data
  for (int i = 0; i < len; ++i)
    if (xferByte(0) != 0xFF) {
      disable();
      return 0;
    }
  disable();
  return 1;
}
  
void df_readBytes (int addr, void* buf, int len) {
  enableCmdAddr(0x03, addr); // Read Data
  for (int i = 0; i < len; ++i)
    ((char*) buf)[i] = xferByte(0);
  disable();
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

#define chunkBits 7
#define chunkSize (1 << chunkBits)
#define chunkMask (chunkSize - 1)

typedef struct {
  uint32_t tag;
  uint16_t start;
  uint16_t crc;
  uint32_t size;
  uint16_t prevFile;
  uint16_t prevStart;
  uint32_t cursor;
  uint16_t spare[6];
} FileEntry;

static FileEntry inFile, outFile, scanFile;
static char lineBuf [100];
static uint16_t freeChunk;
static uint16_t lastFile;

static unsigned chunkToPos (int chunk) {
  return chunk << chunkBits;
}

static void readFileEntry (int chunk, FileEntry* pEntry) {
  int pos = chunkToPos(chunk) + chunkSize - sizeof (FileEntry);
  df_readBytes(pos, pEntry, sizeof (FileEntry));
}

unsigned df_scan (int* pPos) {
  if (*pPos == 0)
    *pPos = lastFile;
  if (*pPos == 0)
    return 0;
  readFileEntry(*pPos, &scanFile);
  *pPos = scanFile.prevFile;
  return scanFile.tag;
}

int df_info (unsigned tag, int* pSize, int* pCrc) {
  int cursor = 0;
  do
    if (df_scan(&cursor) == tag) {
      if (pSize)
        *pSize = scanFile.size;
      if (pCrc)
        *pCrc = scanFile.crc;
      break;
    }
  while (cursor != 0);
  return cursor;
}

void df_open (unsigned tag) {
  if (df_info(tag, 0, 0))
    inFile = scanFile;
  else
    inFile.size = -1;
  inFile.cursor = 0;
}

void df_nextBytes (void* buf, int count) {
  int bytes = inFile.size - inFile.cursor;
  if (bytes > count)
    bytes = count;
  df_readBytes(inFile.cursor, buf, bytes);
  inFile.cursor += bytes;
  count -= bytes;
  buf = (char*) buf + bytes;
  memset(buf, 0xFF, count); // fill bytes past end with 0xFF's
}

const char* df_nextLine (void) {
  if (inFile.cursor >= inFile.size)
    return 0;
  df_nextBytes(lineBuf, sizeof lineBuf);
  lineBuf[sizeof lineBuf - 1] = '\n';
  for (int i = 0; i < sizeof lineBuf; ++i)
    if (lineBuf[i] == '\n' || lineBuf[i] == 0xFF) {
      lineBuf[i] = 0;
      // fix the seek pointer, since we've read too much
      inFile.cursor -= sizeof lineBuf - i - 1;
      return lineBuf;
    }
  return 0;
}

void df_create (unsigned tag) {
  memset(&outFile, 0, sizeof outFile);
  outFile.tag = tag;
  outFile.crc = ~0;
  outFile.start = freeChunk;
}

void df_appendBytes (const void* buf, int count) {
  while (count > 0) {
    int pos = chunkToPos(outFile.start) + outFile.cursor;
    int remain = chunkSize - (pos & chunkMask);
    if (remain > count)
      remain = count;
    /* df_writeBytes(pos, buf, remain); */
    printf("wb %d #%d\n", pos, remain);
    outFile.cursor += remain;
    count -= remain;
    buf = (const char*) buf + remain;
  }
}

void df_appendLine (const char* buf) {
  int length = 0;
  while (buf[length] >= ' ' || buf[length] == '\t')
    ++length;
  df_appendBytes(buf, length);
  df_appendBytes("\n", 1);
}

void df_close (void) {
  int pos = chunkToPos(outFile.start) + outFile.cursor;
  int remain = chunkSize - (pos & chunkMask);
  int fpos = pos + remain - sizeof outFile;
  if (remain < sizeof outFile)
    fpos += chunkSize;
  /* df_writeBytes(fpos, &outFile, sizeof outFile); */
  printf("cl %d @ %d\n", fpos, fpos >> chunkBits);
}
