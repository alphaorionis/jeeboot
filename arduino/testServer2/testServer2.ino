/// @dir testServer
/// Test server for over-the-air updating nodes, sends different blink sketches.
// 2012-10-24 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php

#include <JeeLib.h>
// #include <JeeBoot.h>
#include <avr/pgmspace.h>
#include <util/crc16.h>

#include "data_blinks.h"

#define BOOT_DATA_MAX 64
#include "packet.h"

#define DEBUG 1

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

#if 0
BootReply bootReply;
DataReply dataReply;

byte sectNum;

static void sendReply (const void* ptr, byte len) {
  bitSet(DDRB, 1);
  bitClear(PORTB, 1);
  rf12_sendStart(RF12_ACK_REPLY, ptr, len);
  rf12_sendWait(2);
  bitSet(PORTB, 1);
}

static void initialRequest (word rid) {
  sectNum = 1 - sectNum; // send a different sketch out each time

  bootReply.remoteID = 0;
  bootReply.sketchBlocks = (sections[sectNum].count + 63) / 64;
  word len = 64 * bootReply.sketchBlocks;
  bootReply.sketchCRC = calcCRCrom(progdata + sections[sectNum].off, len);
  
#if DEBUG        
  Serial.print("ir ");
  Serial.print(rid);
  Serial.print(" sb ");
  Serial.print(bootReply.sketchBlocks);
  Serial.print(" crc ");
  Serial.println(bootReply.sketchCRC);
  Serial.flush();
#endif

#if DEBUG > 1
  Serial.print(" -> ack ");
  Serial.println(bootReply.sketchBlocks, DEC);
  
  Serial.print("  send:");
  for (byte i = 0; i < sizeof bootReply; ++i) {
    Serial.print(' ');
    Serial.print(((byte*) &bootReply)[i], HEX);
  }
  Serial.println();
#endif
      
  sendReply(&bootReply, sizeof bootReply);
}

static void dataRequest (word rid, word blk) {
  dataReply.info = blk ^ rid;
  word pos = 64 * blk;

#if DEBUG        
  Serial.print("dr ");
  Serial.print(rid);
  Serial.print(" # ");
  Serial.print(blk);
#if DEBUG > 1
  Serial.print(" -> ");
  Serial.print(dataReply.info);
  Serial.print(" @ ");
  Serial.print(pos);
#endif
  Serial.println();
  Serial.flush();
#endif
  
  for (byte i = 0; i < 64; ++i) {
    byte d = pgm_read_byte(progdata + sections[sectNum].off + pos + i);
    dataReply.data[i] = d; // ^ ...
  }

  sendReply(&dataReply, sizeof dataReply);
}
#endif

static void* memcpy(void* dst, const void* src, int len) {
  uint8_t* to = (uint8_t*) dst;
  const uint8_t* from = (const uint8_t*) src;
  while (--len >= 0)
    *to++ = *from++;
  return dst;
}

void setup () {
#if DEBUG        
  Serial.begin(57600);
  Serial.println("\n[testServer2]");
#endif
  rf12_initialize(31, RF12_868MHZ, 212);
}

// struct { const char* title; unsigned start, off, count; } sections[];
// const unsigned char progdata[] PROGMEM = ...

void loop () {
  if (rf12_recvDone() && rf12_crc == 0 &&
      (rf12_hdr & RF12_HDR_CTL) && (rf12_hdr & RF12_HDR_ACK)) {
    switch (rf12_len) {
      default:
        Serial.print(F("bad length: "));
        Serial.println(rf12_len);
        break;
      case 22: {
        struct PairingRequest *reqp = (struct PairingRequest*) rf12_data;
        Serial.print(F("announce type 0x"));
        Serial.println(reqp->type, HEX);
        static struct PairingReply reply;
        memset(&reply, 0, sizeof reply);
        reply.type = reqp->type;
        reply.group = 212;
        reply.nodeId = 17;
        // memcpy(reply.shKey, "FEDCBA09876543210", sizeof reply.shKey);
        while (!rf12_canSend())
          rf12_recvDone();
        // delay(100);
        rf12_sendNow(0, &reply, sizeof reply);
        break;
      }
      case 8: {
        struct UpgradeRequest *reqp = (struct UpgradeRequest*) rf12_data;
        Serial.print(F("boot swId 0x"));
        Serial.println(reqp->swId, HEX);
        const byte maxSections = sizeof sections / sizeof *sections;
        byte newId = (reqp->swId + 1) % maxSections;
        Serial.print(F(" -> newId "));
        Serial.println(newId);
        static struct UpgradeReply reply;
        memset(&reply, 0, sizeof reply);
        reply.type = reqp->type;
        reply.swId = newId;
        reply.swSize = (sections[newId].count + 15) >> 4;
        reply.swCheck =  calcCRCrom(progdata + sections[newId].off,
                                                    reply.swSize << 4);
        // delay(100);
        rf12_sendNow(RF12_HDR_DST | 17, &reply, sizeof reply);
        break;
      }
      case 4: {
        struct DownloadRequest *reqp = (struct DownloadRequest*) rf12_data;
        Serial.print(F("data swIndex "));
        Serial.println(reqp->swIndex);
        byte reqId = reqp->swId;
        word off = sections[reqId].off + reqp->swIndex * BOOT_DATA_MAX;
        static struct DownloadReply reply;
        reply.swIdXor = reqp->swId ^ reqp->swIndex;
        // memset(reply.data, reqp->swIndex, sizeof reply.data);
        for (byte i = 0; i < BOOT_DATA_MAX; ++i)
          reply.data[i] = pgm_read_byte(progdata + off + i) ^ (211 * i);
        // introduce random errors in last code section
        static byte random = 1;
        if (reqId == 2) {
          random *= 211;
          // if (random > 100)
          //   break; // no reply
        }
        // delay(100);
        rf12_sendNow(RF12_HDR_DST | 17, &reply, sizeof reply);
        break;
      }
    }
    // const word* args = (const word*) rf12_data;
    // if (rf12_len == 2)
    //   initialRequest(args[0]);
    // else if (rf12_len == 4)
    //   dataRequest(args[0], args[1]);
  }
}
