/**
I took Andrew Rapp's receive example and modified it to be completely 
unrecognizable.  

I wanted to experiment with and better understand how to use his XBee 
library for an actual project.  With the inclusion of support for SoftwareSerial 
so that the XBee can be put on digital pins leaving the Arduino serial port
available for debugging and status, the library's usefullness shot way up.

This is a HEAVILY commented example of how to grab a receive packet off the 
air and do something with it using series 2 XBees.  Series 1 XBees are left as
and exercise for the student.
*/
 
#include <XBee.h>
#include <SoftwareSerial.h>

XBee xbee = XBee();
XBeeResponse response = XBeeResponse();
// create reusable response objects for responses we expect to handle 
ZBRxResponse rx = ZBRxResponse();

// Define NewSoftSerial TX/RX pins
// Connect Arduino pin 2 to Tx and 3 to Rx of the XBee
// I know this sounds backwards, but remember that output
// from the Arduino is input to the Xbee
#define ssRX 2
#define ssTX 3
SoftwareSerial nss(ssRX, ssTX);


void setup() {  
  // start serial
  Serial.begin(9600);
  Serial3.begin(9600);
  // and the software serial port
//  nss.begin(9600);
  // now that they are started, hook the XBee into 
  // Software Serial
  xbee.setSerial(Serial3);
  // I think this is the only line actually left over
  // from Andrew's original example
  Serial.println("starting up yo!");
}

void loop() {
    // doing the read without a timer makes it non-blocking, so
    // you can do other stuff in loop() as well.
    xbee.readPacket();
    // so the read above will set the available up to 
    // work when you check it.
    if (xbee.getResponse().isAvailable()) {
      // got something
      Serial.println();
      Serial.print("Frame Type is ");
      // Andrew call the frame type ApiId, it's the first byte
      // of the frame specific data in the packet.
      Serial.println(xbee.getResponse().getApiId(), HEX);
      
      if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
        // got a zb rx packet, the kind this code is looking for
        
        // now that you know it's a receive packet
        // fill in the values
        xbee.getResponse().getZBRxResponse(rx);
        
        // this is how you get the 64 bit address out of
        // the incoming packet so you know which device
        // it came from
        Serial.print("Got an rx packet from: ");
        XBeeAddress64 senderLongAddress = rx.getRemoteAddress64();
        print32Bits(senderLongAddress.getMsb());
        Serial.print(" ");
        print32Bits(senderLongAddress.getLsb());
        
        // this is how to get the sender's
        // 16 bit address and show it
        uint16_t senderShortAddress = rx.getRemoteAddress16();
        Serial.print(" (");
        print16Bits(senderShortAddress);
        Serial.println(")");
        
        // The option byte is a bit field
        if (rx.getOption() & ZB_PACKET_ACKNOWLEDGED)
            // the sender got an ACK
          Serial.println("packet acknowledged");
        if (rx.getOption() & ZB_BROADCAST_PACKET)
          // This was a broadcast packet
          Serial.println("broadcast Packet");
          
        Serial.print("checksum is ");
        Serial.println(rx.getChecksum(), HEX);
        
        // this is the packet length
        Serial.print("packet length is ");
        Serial.print(rx.getPacketLength(), DEC);
        
        // this is the payload length, probably
        // what you actually want to use
        Serial.print(", data payload length is ");
        Serial.println(rx.getDataLength(),DEC);
        
        // this is the actual data you sent
        Serial.println("Received Data: ");
        for (int i = 0; i < rx.getDataLength(); i++) {
          print8Bits(rx.getData()[i]);
          Serial.print(' ');
        }
        
        // and an ascii representation for those of us
        // that send text through the XBee
        Serial.println();
        for (int i= 0; i < rx.getDataLength(); i++){
          Serial.write(' ');
          if (iscntrl(rx.getData()[i]))
            Serial.write(' ');
          else
            Serial.write(rx.getData()[i]);
          Serial.write(' ');
        }
        Serial.println();
        
        // So, for example, you could do something like this:
        handleXbeeRxMessage(rx.getData(), rx.getDataLength());
/*
        // I commented out the printing of the entire frame, but
        // left the code in place in case you want to see it for
        // debugging or something
        Serial.println("frame data:");
        for (int i = 0; i < xbee.getResponse().getFrameDataLength(); i++) {
          print8Bits(xbee.getResponse().getFrameData()[i]);
          Serial.print(' ');
        }
        Serial.println();
        for (int i= 0; i < xbee.getResponse().getFrameDataLength(); i++){
          Serial.write(' ');
          if (iscntrl(xbee.getResponse().getFrameData()[i]))
            Serial.write(' ');
          else
            Serial.write(xbee.getResponse().getFrameData()[i]);
          Serial.write(' ');
        }
        Serial.println();
*/
      }
      else {
        Serial.print("Got frame id: ");
        Serial.println(xbee.getResponse().getApiId(), HEX);
      }
    } 
    else if (xbee.getResponse().isError()) {
      // some kind of error happened, I put the stars in so
      // it could easily be found
      Serial.print("************************************* error code:");
      Serial.println(xbee.getResponse().getErrorCode(),DEC);
    }
    else {
      // I hate else statements that don't have some kind
      // ending.  This is where you handle other things
    }
}

void handleXbeeRxMessage(uint8_t *data, uint8_t length){
  // this is just a stub to show how to get the data,
  // and is where you put your code to do something with
  // it.
  for (int i = 0; i < length; i++){
//    Serial.print(data[i]);
  }
//  Serial.println();
}

// these routines are just to print the data with
// leading zeros and allow formatting such that it 
// will be easy to read.
void print32Bits(uint32_t dw){
  print16Bits(dw >> 16);
  print16Bits(dw & 0xFFFF);
}

void print16Bits(uint16_t w){
  print8Bits(w >> 8);
  print8Bits(w & 0x00FF);
}
  
void print8Bits(byte c){
  uint8_t nibble = (c >> 4);
  if (nibble <= 9)
    Serial.write(nibble + 0x30);
  else
    Serial.write(nibble + 0x37);
        
  nibble = (uint8_t) (c & 0x0F);
  if (nibble <= 9)
    Serial.write(nibble + 0x30);
  else
    Serial.write(nibble + 0x37);
}
