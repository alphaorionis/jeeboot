/// @dir testBlink1
/// Slow blink, test app for over-the-air uploading.
// 2012-10-01 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

volatile char dummy;

void setup () {
  DDRB |= 1 << 1;
}

void loop () {
  PINB = 1 << 1;
  for (long i = 0; i < 500000 && !dummy; ++i)
    ;
}
