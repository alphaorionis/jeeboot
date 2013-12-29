// 
// Over-The-Air download nd programming logic
//
// The code here is somewhat Arduino-specific

#define BOOT_DATA_MAX 64									// max bytes found in a boot packet
#include "packet.h"												// packet format definitions

#define PAGE_SIZE SPM_PAGESIZE          	// minimal chunk written to flash (128 on Atmega328p)
#define BASE_ADDR ((uint8_t*) 0x0)			  // base address of user program
#define CONFIG_ADDR (BASE_ADDR - sizeof(config)) // where config goes

static uint16_t calcCRC (const uint8_t *ptr, int len) {
  int crc = ~0;
	while (len--)
    crc = _crc16_update(crc, *ptr++);
  //P("  crc "); P_X16(crc); P_LN();
  return crc;
}

// calculate the CRC of a block in flash, len must be a multiple of 2!
static uint16_t calcFlashCRC (const uint8_t *ptr, int len) {
  uint16_t crc = ~0;
	while (len--) {
		uint8_t b = pgm_read_byte_near(ptr++);
    crc = _crc16_update(crc, b);
	}
  //P("  crc "); P_X16(crc); P_LN();
  return crc;
}

//===== Communication =====

// return 1 if good reply, 0 if crc error, -1 if timeout
static int sendRequest (const void* buf, int len, int hdrOr) {
  P("SEND "); P_I8(len); P(" -> ");
  rf12_sendNow(RF12_HDR_CTL | RF12_HDR_ACK | hdrOr, buf, len);
  rf12_sendWait(0);
  uint32_t now = millis();
  while (!rf12_recvDone() || rf12_len == 0) // TODO: 0-check to avoid std acks?
    if ((millis() - now) >= 250) {
      P("timeout\n");
      return -1;
    }
  if (rf12_crc) {
    P("bad crc "); P_X16(rf12_crc); P_LN();
    return 0;
  }
  P_I8(rf12_len); P(" hdr=0x"); P_X8(rf12_hdr); P_LN();
  return 1;
}

//===== writing to program memory flash =====

// Buffer to accumulate data packets untilwe can write a full page. Allocate  bit extra
// to allow for odd radio packet sizes since we've got enough RAM...
static uint16_t flashBuffer[(PAGE_SIZE+BOOT_DATA_MAX+1)/2];   // buffer for a full page of flash

// Write a complete buffer to flash
// TODO: optimize for RWW section to erase while requesting data
// TODO: use boot.h from optiboot 'cause it's faster and smaller
static void writeFlash(void *flash) {
	//P("FW "); P_X16((uint16_t)flash); P_LN();
	//P_A(flashBuffer, PAGE_SIZE); P_LN();
  // first erase the page
	boot_page_erase(flash);
	boot_spm_busy_wait();
	// copy the in-memory buffer into the write-buffer
	for (uint8_t i=0; i<PAGE_SIZE/2; i++) {
		boot_page_fill(flash+2*i, flashBuffer[i]);
	}
	boot_page_write(flash);
	boot_spm_busy_wait();
	boot_rww_enable();
}

// copy a chunk from memory into the flash buffer and write flash if we've got a page full
static void fillFlash (void *flash, uint8_t *ram, uint8_t sz) {
	//P("FF "); P_X16((uint16_t)flash); P_LN();
	// copy ram to buffer
	uint16_t offset = (uint16_t)flash & (PAGE_SIZE-1);
	memcpy(flashBuffer+offset/2, ram, sz);
	// time to to flash?
	if (offset+sz >= PAGE_SIZE) {
		writeFlash(flash-offset);
		// shift excess data down
		memcpy(flashBuffer, flashBuffer+PAGE_SIZE/2, offset+sz-PAGE_SIZE);
	}
}

// flush what's left in the buffer, argument is address of next byte we would have written to
// buffer, i.e., address in flash of byte after the last one present in buffer
static void flushFlash(void *flash) {
	uint16_t offset = (uint16_t)flash & (PAGE_SIZE-1);
	//P("FL "); P_X16((uint16_t)flash); P_LN();
	if (offset != 0) {
		memset(flashBuffer+offset/2, 0xFF, PAGE_SIZE-offset);   // fill rest of buffer with 1's
		writeFlash(flash-offset);
	}
}

//===== exponential back-off =====

// retry up to 16 times to get a response, this make take several hours due to
// the exponential back-off logic, which starts with a 250 ms timeout period
// it'll stick to the maximum 4.5 hour cycle once the limit has been reached
// without server, this'll listen for 250 ms up to 85x/day = 21 s = 0.25% duty

static byte backOffCounter;

static void exponentialBackOff () {
  P("  backoff "); P_I8(backOffCounter); P_LN();
  sleep(250L << backOffCounter);
  if (backOffCounter < 16)
    ++backOffCounter;
}

//===== Config =====

// The config stores the vital information about the node's identify and software
// It is saved in program flash at the end of the bootloader, i.e., just below
// 0x1000 (4KB) for an Atmega328p. We have 64 bytes reserved for this.

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

static void loadConfig () {
  // copy config from program memory to config struct
	for (uint8_t i=0; i<sizeof(config); i++) {
		((uint8_t*)&config)[i] = pgm_read_byte_near(CONFIG_ADDR+i);
	}
	P_A(&config, sizeof config); P_LN();
	// calculate checksum to verify it's valid
  if (calcCRC(&config, sizeof config) != 0) {
    P("DEF!\n");
    memset(&config, 0, sizeof config);
  }
}

// Saves config by inserting it into the end of the last page program memory (flash)
static void saveConfig () {
  config.version = 1;
  if (calcCRC(&config, sizeof config) != 0) {
    config.check = calcCRC(&config, sizeof config - 2);
    //P("save config 0x"); P_X16(config.check); P_LN();
		//P("config @0x"); P_A(&config, sizeof(config));
		// Load last page of program memory
		uint8_t *tgt = (uint8_t *)flashBuffer;
		for (uint8_t i=0; i<PAGE_SIZE; i++)
				tgt[i] = pgm_read_byte_near(BASE_ADDR-PAGE_SIZE+i);
		// Slap config on top and flash it!
		fillFlash(CONFIG_ADDR, &config, sizeof(config));
  }
}

//===== Pairing =====

static void sendPairingCheck () {
	// form the pairing request message
  struct PairingRequest request;
  request.type = REMOTE_TYPE;
  request.group = config.group;
  request.nodeId = config.nodeId;
  request.check = calcCRC(&config.shKey, sizeof config.shKey);
  memcpy(request.hwId, hwId, sizeof request.hwId);
  
	// send the message and assuming we get a reply, set the config from the reply
  struct PairingReply *reply;
  if (sendRequest(&request, sizeof request, RF12_HDR_DST) > 0 && rf12_len == sizeof(*reply)) {
		reply = (struct PairingReply *)rf12_data;
    config.group = reply->group;
    config.nodeId = reply->nodeId;
    memcpy(config.shKey, reply->shKey, sizeof config.shKey);
    saveConfig();
    P("P id="); P_I8(config.nodeId); P(" g="); P_I8(config.group); P_LN();
  }
}

//===== Upgrade =====

static int appIsValid () {
  //return calcCRC(BASE_ADDR, config.swSize << 4) == config.swCheck;
  uint16_t curr = calcFlashCRC(BASE_ADDR, config.swSize << 4);
	P("SW: curr="); P_X16(curr);
	P(" want="); P_X16(config.swCheck);
	P(curr == config.swCheck ? " OK\n" : " NOPE\n");
	return curr == config.swCheck;
}

static int sendUpgradeCheck () {
	// form upgrade check message
  struct UpgradeRequest request;
  request.type = REMOTE_TYPE;
  request.swId = config.swId;
  request.swSize = config.swSize;
  request.swCheck = config.swCheck;
	// send the message and update the config based on the reply, if we get one
  struct UpgradeReply *reply;
  if (sendRequest(&request, sizeof request, 0) > 0 && rf12_len == sizeof(*reply)) {
		reply = (struct UpgradeReply *)rf12_data;
    config.swId = reply->swId;
    config.swSize = reply->swSize;
    config.swCheck = reply->swCheck;
    saveConfig();
		//P("sw: id="); P_X16(config.swId); P(" sz="); P_X16(config.swSize);
		//P(" crc="); P_X16(config.swCheck); P_LN();
		//P("config @0x"); P_A(&config, sizeof(config));
    return 1;
  }
  return 0;
}

//===== Download =====

static int sendDownloadRequest (int index) {
	// Compose download request
  struct DownloadRequest request;
  request.swId = config.swId;
  request.swIndex = index;
	// Send request and if we got a reply copy it to flash
  if (sendRequest(&request, sizeof request, 0) > 0 &&
			rf12_len == sizeof(struct DownloadReply) &&
			*(uint16_t*)rf12_data == (request.swId ^ request.swIndex)) // check reply.swIdXor
	{
		// de-whitening (prevents simple runs of all-0 or all-1 bits)
    for (int i = 0; i < BOOT_DATA_MAX; ++i)
      rf12_data[2+i] ^= 211 * i;
    void* flash = BASE_ADDR + BOOT_DATA_MAX * index;
    fillFlash(flash, rf12_data+2, BOOT_DATA_MAX);
		P("F "); P_X8(request.swIndex); P(" @"); P_X16((uint16_t)flash); P_LN();
    return 1;
  }
  return 0;
}

//===== Boot process =====

static void bootLoaderLogic () {
  loadConfig();
  
	// Pairing: figure out who we're supposed to communicate with (and boot from)
  rf12_initialize(1, RF12_915MHZ, PAIRING_GROUP);

  P("==P\n");
  backOffCounter = 0;
  while (1) {
    sendPairingCheck();
    if (config.group != 0 && config.nodeId != 0) // paired
      break;
    exponentialBackOff();
  }
  
	// Upgrade check: figure out whether we have the right sketch loaded
  rf12_initialize(config.nodeId, RF12_915MHZ, config.group);

  P("==U\n");
  backOffCounter = 0;
  do {
    if (sendUpgradeCheck())
      break;
    exponentialBackOff();
  } while (! appIsValid());
  
	// Download: if the app we have is not the right one then download the right one
  P("==D\n");
  if (! appIsValid()) {
    int limit = ((config.swSize << 4) + BOOT_DATA_MAX - 1) / BOOT_DATA_MAX;
    for (int i = 0; i < limit; ++i) {
      backOffCounter = 0;
      while (sendDownloadRequest(i) == 0)
        exponentialBackOff();
    }
		flushFlash(BASE_ADDR + BOOT_DATA_MAX*limit);
  }

  P("==R!\n");
}

static void bootLoader () {
  sleep(20); // needed to make RFM69 work properly on power-up
  
  // this will not catch the runaway case when the server replies with data,
  // but the application that ends up in memory does not match the crc given
  // in this case, we'll constantly keep retrying... and drain the battery :(
  // to avoid this, an extra level of exponential back-off has been added here
  for (int backOff = 0; /*forever*/; ++backOff) {
    bootLoaderLogic();
    if (appIsValid())
      break;
		P("  WRONG APP!\n");
    sleep(100L << (backOff & 0x0F));
  }
}
