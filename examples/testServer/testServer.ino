/// @dir testServer
/// Test server for over-the-air updating nodes, sends different blink sketches.
// 2012-10-24 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php

#include <JeeLib.h>
#include <JeeBoot.h>
#include <avr/pgmspace.h>
#include <util/crc16.h>

#include "data_blinks.h"

#define DEBUG 1

BootReply bootReply;
DataReply dataReply;

byte sectNum;

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

void setup () {
#if DEBUG        
    Serial.begin(57600);
    Serial.println("\n[testServer]");
#endif
    rf12_initialize(1, RF12_868MHZ, 254);
}

void loop () {
    if (rf12_recvDone() && rf12_crc == 0 && RF12_WANTS_ACK) {
        const word* args = (const word*) rf12_data;
        if (rf12_len == 2)
            initialRequest(args[0]);
        else if (rf12_len == 4)
            dataRequest(args[0], args[1]);
    }
}
