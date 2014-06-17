/// @dir testServer
/// Test server for over-the-air updating nodes, sends different blink sketches.
// 2012-10-24 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php

#include <JeeLib.h>
#include <util/crc16.h>

#define PAIRING_GROUP 212
#define BOOT_GROUP PAIRING_GROUP

// struct { const char* title; unsigned start, off, count; } sections[];
// const unsigned char progdata[] PROGMEM = ...
#include "blink.h"

#define BOOT_DATA_MAX 64
#include "packet.h"

#define DEBUG 1

byte newId = 0;

static word calcCRC (const void* ptr, word len) {
  word crc = ~0;
  for (word i = 0; i < len; ++i)
    crc = _crc16_update(crc, ((const char*) ptr)[i]);
  return crc;
}

static word calcCRCrom (const void* ptr, word len) {
  word crc = ~0;
  for (word i = 0; i < len; ++i)
    crc = _crc16_update(crc, pgm_read_byte((word) ptr + i));
  return crc;
}

static void* memcpy(void* dst, const void* src, int len) {
  uint8_t* to = (uint8_t*) dst;
  const uint8_t* from = (const uint8_t*) src;
  while (--len >= 0)
    *to++ = *from++;
  return dst;
}

static void print2hex (byte h) {
  Serial.print(h >> 4, HEX);
  Serial.print(h % 0xF, HEX);
}

void setup () {
#if DEBUG        
  Serial.begin(57600);
  Serial.println("\n[testServer2]");
#endif
  rf12_initialize(31, RF12_868MHZ, PAIRING_GROUP);
}

#define THROTTLE 5

void loop () {
    // boot loader request packets haev a special header with CTL *and* ACK set
  if (rf12_recvDone()) {    
    Serial.println("received packet");
    
    if (rf12_crc == 0) {      
      Serial.println("crc ok");
      
      if ((rf12_hdr & RF12_HDR_CTL) && (rf12_hdr & RF12_HDR_ACK)) {
        Serial.println("headers ok");
        Serial.println(rf12_len);
        
    switch (rf12_len) {
        
      case 22: { // packets of length 22 are pairing requests
        Serial.print(F("pairing request received, nodeId: "));
        struct PairingRequest *reqp = (struct PairingRequest*) rf12_data;
        Serial.print(reqp->nodeId);
        Serial.print(F(" announce type 0x"));
        Serial.print(reqp->type, HEX);
        Serial.print(F(" hwId "));
        for (byte i = 0; i < 16; ++i)
          print2hex(reqp->hwId[i]);
        Serial.println();
        static struct PairingReply reply;
        memset(&reply, 0, sizeof reply);
        reply.type = reqp->type;
        reply.group = BOOT_GROUP;
        reply.nodeId = 17;
        // memcpy(reply.shKey, "FEDCBA09876543210", sizeof reply.shKey);
        while (!rf12_canSend())
          rf12_recvDone();
        delay(THROTTLE);
        rf12_sendNow(0, &reply, sizeof reply);
        break;
      }
      
      case 8: { // packets of length 8 are upgrade requests
        Serial.print(F("upgrade request received "));
        struct UpgradeRequest *reqp = (struct UpgradeRequest*) rf12_data;
        Serial.print(F("boot swId 0x"));
        Serial.println(reqp->swId, HEX);
        switch (newId) {
          case 0:
        newId = 1;
        break;
          case 1:
        newId = 0;
        break;
        }
        Serial.print(F(" -> newId "));
        Serial.println(newId);
        static struct UpgradeReply reply;
        memset(&reply, 0, sizeof reply);
        reply.type = reqp->type;
        reply.swId = newId;
        reply.swSize = (sections[newId].count + 15) >> 4;
        reply.swCheck =  calcCRCrom(progdata + sections[newId].off,
                                                reply.swSize << 4);
        delay(THROTTLE);
        rf12_sendNow(RF12_HDR_DST | 17, &reply, sizeof reply);
        break;
      }
      
      case 4: { // packets of length 4 are download requests
        Serial.print(F("download request received "));
        struct DownloadRequest *reqp = (struct DownloadRequest*) rf12_data;
        Serial.print(F("data swId "));
        Serial.print(reqp->swId);
        Serial.print(F(", index "));
        Serial.println(reqp->swIndex);
        byte reqId = reqp->swId;
        word off = sections[reqId].off + reqp->swIndex * BOOT_DATA_MAX;
        static struct DownloadReply reply;
        reply.swIdXor = reqp->swId ^ reqp->swIndex;
        // memset(reply.data, reqp->swIndex, sizeof reply.data);
        for (byte i = 0; i < BOOT_DATA_MAX; ++i)
          reply.data[i] = pgm_read_byte(progdata + off + i) ^ (211 * i);
        // introduce random errors in the blue code section by not responding
        // static byte random = 1;
        // if (reqId == 2) {
        //   random *= 211;
        //   if (random >= 128)
        //     break; // no reply
        // }
        delay(THROTTLE);
        rf12_sendNow(RF12_HDR_DST | 17, &reply, sizeof reply);
        break;
      }
      
      default:
        Serial.print(F("bad length: "));
        Serial.println(rf12_len);
        break;
    }
      }
    }
  }
}
