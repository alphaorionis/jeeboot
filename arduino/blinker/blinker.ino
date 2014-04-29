#define BLINKS 2
#define RESET 3

#include <avr/wdt.h>

void setup () {
  // PB1 = digital 9 = JN ISP.B1
  bitSet(PORTB, 1);
  bitSet(DDRB, 1);
  Serial.begin(57600);
  Serial.println("Hello!");
}

byte reset=RESET;

void loop () {
  for (byte i = 0; i < 2 * BLINKS; ++i) {
    PINB = bit(1); // toggles PORTB!
    delay(100);
  }
  delay(500);
  if (--reset == 0) {
	  MCUSR = 0xff; // set the EXTRF flag
          Serial.print("MCUSR=");
	  byte m = MCUSR;
	  Serial.println(m);
	  wdt_enable(WDTO_120MS);
	  //digitalWrite(6, HIGH);
	  //pinMode(6, OUTPUT);
	  //digitalWrite(6, LOW);
	  for (;;) ;
  }
}
