#include <avr/wdt.h>

void setup(){
  Serial.begin(57600);
  Serial.println("Hello, in setup !!!");
}

int firstTime = true;

void loop() {
      if (firstTime){
        firstTime = false;
        Serial.println("In loop");
      }
      // now just do nothing
}

