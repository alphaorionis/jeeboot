#define BLINKS 1

void setup () {
  // PB1 = digital 9 = JN ISP.B1
  bitSet(PORTB, 1);
  bitSet(DDRB, 1);
}

void loop () {
  for (byte i = 0; i < 2 * BLINKS; ++i) {
    PINB = bit(1); // toggles PORTB!
    delay(100);
  }
  delay(500);
}