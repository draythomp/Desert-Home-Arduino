/*
XBee RX test for a Arduino Mega2560 using Serial3 as the XBee serial
input for a Series 2 XBee.  This is NOT based on the examples that come with
the Arduino XBee library.

See, the examples there and most other places on the web SUCK.  Andrew's 
library is much easier to use than the illustrations would lead you to believe.

This is a HEAVILY commented example of how send a text packet using series 2 
XBees.  Series 1 XBees are left as an exercise for the student.
*/
 
#include <XBee.h>

XBee xbee = XBee();
// This is the XBee broadcast address.  You can use the address 
// of any device you have also.
XBeeAddress64 Broadcast = XBeeAddress64(0x00000000, 0x0000ffff);

char Hello[] = "Hello World\r";
char Buffer[128];  // this needs to be longer than your longest packet.

void setup() {  
  // start serial
  Serial.begin(9600);
  // and the software serial port
  Serial3.begin(9600);
  // now that they are started, hook the XBee into 
  // Software Serial
  xbee.setSerial(Serial3);
  // I think this is the only line actually left over
  // from Andrew's original example
  Serial.println("Initialization all done!");
}

void loop() {
  ZBTxRequest zbtx = ZBTxRequest(Broadcast, (uint8_t *)Hello, strlen(Hello));
  xbee.send(zbtx);
  delay(30000);
  strcpy(Buffer,"I saw what you did last night.\r");
  zbtx = ZBTxRequest(Broadcast, (uint8_t *)Buffer, strlen(Buffer));
  xbee.send(zbtx);
  delay(30000);
}
