#include <avr/wdt.h>

void setup(){

  wdt_reset();   // First thing, turn it off
  wdt_disable();
  Serial.begin(9600);
  Serial.println("Hello, in setup!!!");
}

int firstTime = true;

void loop() {
      if (firstTime){
        firstTime = false;
        Serial.println("In loop waiting for Watchdog");
        wdt_enable(WDTO_8S);    // eight seconds from now the device will reboot
      }
      // now just do nothing
}

