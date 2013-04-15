/// @dir testBlink2
/// Fast blink, test app for over-the-air uploading.
// 2012-10-01 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

volatile char dummy;

void setup () {
  DDRB |= 1 << 1;
}

static void toggleWait (long cycles) {
  PINB = 1 << 1;
  while (--cycles >= 0 && !dummy)
    ;
}

void loop () {
  toggleWait(100000);
}
