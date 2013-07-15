#include <JeeLib.h>
#include <JeeBoot.h>

PairRequest reqBuf;
MilliTimer pairTimer;
PairReply* reply;

void setup () {
  Serial.begin(57600);
  Serial.println("\n[pairRemote]");
  rf12_initialize(1, RF12_868MHZ, 212);
}

void loop () {
  rf12_sendNow(RF12_HDR_DST, &reqBuf, sizeof reqBuf);

  pairTimer.set(250);
  reply = 0;
  while (!pairTimer.poll())
    if (rf12_recvDone() && rf12_crc == 0 && rf12_len >= sizeof (PairReply)) {
      Serial.println("got it");
      reply = (PairReply*) rf12_data;
    }
  if (reply == 0)
    Serial.println("reply?");

  delay(2000);
}
