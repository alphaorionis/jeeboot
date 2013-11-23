#define BOOT_DATA_MAX 64

#include "packet.h"

#define PAGE_SIZE 64
#define SECTOR_SIZE (PAGE_SIZE * 16)
#define BASE_ADDR ((uint8_t*) 0x1000)
#define BASE_PAGE ((uint32_t) BASE_ADDR / PAGE_SIZE)

static void* memset(void* dst, uint8_t fill, int len) {
  uint8_t* to = dst;
  while (--len >= 0)
    *to++ = fill;
  return dst;
}

static void* memcpy(void* dst, const void* src, int len) {
  uint8_t* to = dst;
  const uint8_t* from = src;
  while (--len >= 0)
    *to++ = *from++;
  return dst;
}

static uint16_t calcCRC (const void* ptr, int len) {
  int crc = ~0;
  for (uint16_t i = 0; i < len; ++i)
    crc = _crc16_update(crc, ((const char*) ptr)[i]);
  printf("crc %04X\n", crc);
  return crc;
}

#ifndef dump
static void dump (const char* msg, const uint8_t* buf, int len) {
  printf("%s #%d:", msg, len);
  for (int i = 0; i < len; ++i) {
    if (i % 32 == 0)
      printf("\n ");
    if (i % 4 == 0)
      printf(" ");
    printf("%02X", buf[i]);
  }
  printf("\n");
}
#endif

// return 1 if good reply, 0 if crc error, -1 if timeout
static int sendRequest (const void* buf, int len) {
  dump("send", buf, len);
  rf12_sendNow(RF12_HDR_CTL | RF12_HDR_ACK, buf, len);
  uint32_t now = msTicks;
  while (!rf12_recvDone())
    if ((msTicks - now) >= 250) {
      printf("timed out\n");
      return -1;
    }
  if (rf12_crc) {
    printf("bad crc %04X\n", rf12_crc);
    return 0;
  }
  dump("recv", (const uint8_t*) rf12_data, rf12_len);
  return 1;
}

static void copyPageToFlash (void* ram, void* flash) {
  int page = (uint32_t) flash / PAGE_SIZE;
  int sect = (uint32_t) flash / SECTOR_SIZE;
  printf("ram 0x%X flash 0x%X page %d sect %d ",
          (int) ram, (int) flash, page, sect);
  iap_prepare_sector(sect, sect);
  int e1 = iap_erase_page(page, page);
  printf("iap erase %d,", e1);
  iap_prepare_sector(sect, sect);
  int e2 = iap_copy_ram_to_flash(ram, flash, PAGE_SIZE);
  printf(" flash %d\n", e2);
}

int backOffCounter;

struct Config {
  uint32_t version;
  uint8_t group;
  uint8_t nodeId;
  uint8_t spare [2];
  uint8_t shKey [16];
  uint16_t swId;
  uint16_t swSize;
  uint16_t swCheck;
  uint16_t check;
} config;

#define TOP_OF_BOOT ((uint8_t*) 0x1000)
#define CONFIG_ADDR (TOP_OF_BOOT - PAGE_SIZE)

static void saveConfig () {
  config.version = 1;
  config.check = calcCRC(&config, sizeof config - 2);
  printf("save config 0x%X\n", config.check);
  copyPageToFlash(&config, CONFIG_ADDR);
}

static int loadValidConfig () {
  memcpy(&config, CONFIG_ADDR, sizeof config);
  return calcCRC(&config, sizeof config) == 0;
}
  
static void setDefaultConfig () {
  printf("default config\n");
  memset(&config, 0, sizeof config);
}

static void sendIdentityCheck () {
  uint32_t hwId [4];
  iap_read_unique_id(hwId);
  struct PairingRequest request;
  request.type = 0x0200; // LPC812
  request.group = config.group;
  request.nodeId = config.nodeId;
  request.check = calcCRC(&config.shKey, sizeof config.shKey);
  memcpy(request.hwId, hwId, sizeof request.hwId);
  // memcpy(request.hwId,
  //   "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF", 16);
  struct PairingReply reply;
  if (sendRequest(&request, sizeof request) > 0 && rf12_len == sizeof reply) {
    memcpy(&reply, (const void*) rf12_data, sizeof reply);
    config.group = reply.group;
    config.nodeId = reply.nodeId;
    memcpy(config.shKey, reply.shKey, sizeof config.shKey);
    saveConfig();
    printf("paired group %d node %d\n", config.group, config.nodeId);
    rf12_initialize(config.nodeId, RF12_868MHZ, config.group);
  }
}

// retry up to 16 times to get a response, this make take several hours due to
// the exponential back-off logic, which starts with a 250 ms timeout period
// it'll stick to the maximum 4.5 hour cycle once the limit has been reached
// without server, this'll listen for 250 ms up to 85x/day = 21 s = 0.25% duty

static void exponentialBackOff () {
  printf("wait %d\n", 250 << backOffCounter);
  delay_ms(250 << backOffCounter);
  if (backOffCounter < 16)
    ++backOffCounter;
}

static int appIsValid () {
  return calcCRC(BASE_ADDR, config.swSize << 4) == config.swCheck;
}

static int sendUpgradeCheck () {
  struct UpgradeRequest request;
  request.type = 0x0200; // LPC812
  request.swId = config.swId;
  request.swSize = config.swSize;
  request.swCheck = config.swCheck;
  struct UpgradeReply reply;
  if (sendRequest(&request, sizeof request) > 0 && rf12_len == sizeof reply) {
    memcpy(&reply, (const void*) rf12_data, sizeof reply);
    // ...
    config.swId = reply.swId;
    config.swSize = reply.swSize;
    config.swCheck = reply.swCheck;
    saveConfig();
    return 1;
  }
  return 0;
}

static int sendCodeRequest (uint8_t* fill) {
  struct DownloadRequest request;
  request.swId = config.swId;
  request.swIndex = (fill - BASE_ADDR) / BOOT_DATA_MAX;
  struct DownloadReply reply;
  if (sendRequest(&request, sizeof request) > 0 && rf12_len == sizeof reply) {
    for (int i = 0; i < BOOT_DATA_MAX; ++i)
      rf12_data[2+i] ^= 211 * i;
    // dump("de-whitened", (const void*) rf12_data, rf12_len);
    union { uint32_t longs[16]; uint8_t bytes[64]; } aligned;
    memcpy(aligned.bytes, (const void*) (rf12_data + 2), sizeof aligned);
    void* flash = BASE_ADDR + PAGE_SIZE * request.swIndex;
    copyPageToFlash(aligned.bytes, flash);
    dump("in flash", flash, PAGE_SIZE);
    return 1;
  }
  return 0;
}

static int saveReceivedCode (uint8_t* fill) {
  return BOOT_DATA_MAX;
}

static int bootLoaderLogic () {
  printf("1\n");
  if (! loadValidConfig())
    setDefaultConfig();
  
  printf("2\n");
  backOffCounter = 0;
  while (1) {
    sendIdentityCheck(); // pairing check in group 212, pick up reply if any
    if (config.group != 0 && config.nodeId != 0) // paired
      break;
    exponentialBackOff();
  }
  
  printf("3\n");
  backOffCounter = 0;
  do {
    if (sendUpgradeCheck())
      break;
    exponentialBackOff();
  } while (! appIsValid());
  
  printf("4\n");
  if (! appIsValid()) {
    uint8_t* codeEnd = BASE_ADDR + (config.swSize << 4);
    for (uint8_t *p = BASE_ADDR; p < codeEnd; p += saveReceivedCode(p)) {
      backOffCounter = 0;
      while (sendCodeRequest(p) == 0)
        exponentialBackOff();
    }
    saveReceivedCode(0); // save last bytes
  }

  printf("5\n");
  return 0;
}
