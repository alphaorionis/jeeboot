#include <JeeLib.h>
#include <stdint.h>
#include <util/crc16.h>

#define REMOTE_TYPE 0x100
#define PAIRING_GROUP 212

#define printf(...)
#define dump(...)

#define msTicks millis()
#define delay_ms delay

#include "boot.h"

void setup () {
  Serial.begin(57600);
  bootLoader();
  Serial.println("launch!");
}

void loop () {}