/*
I needed a source of messages for a test of receiving on an
XBee.  At the time, the only board I had handy was a mega2560,
so I slapped this together to send messages every 10 seconds
to test the receive.  I just loaded this on the mega and let
it run while I concentrated on the receive side.
*/
 
#include <XBee.h>
#include <Time.h>
#include <TimeAlarms.h>

XBee xbee = XBee();
// This is the XBee broadcast address.  You can use the address 
// of any device you have also.
XBeeAddress64 Broadcast = XBeeAddress64(0x00000000, 0x0000ffff);
ZBRxResponse xbrx = ZBRxResponse();  // for holding the ascii traffic from the XBee
ZBTxStatusResponse txStatus = ZBTxStatusResponse(); // to look at the tx response packet

char Hello[] = "Hello World\r";
char Buffer[128];  // this needs to be longer than your longest packet.

void pass(){
    return;
}

void sendXBee(){
  Serial.println();
  ZBTxRequest zbtx = ZBTxRequest(Broadcast, (uint8_t *)Hello, strlen(Hello));
  zbtx.setFrameId(0xaa);
  xbee.send(zbtx);
  Serial.println("Sent it");
}

void checkXbee(){
    // doing the read without a timer makes it non-blocking, so
    // you can do other stuff in loop() as well.
    xbee.readPacket();
    // so the read above will set the available up to 
    // work when you check it.
    if (xbee.getResponse().isAvailable()) {
      // got something
      if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
        // got a zb rx packet, the kind this code is looking for
        xbee.getResponse().getZBRxResponse(xbrx);
        Serial.println("got an incoming packet");
      }
      else if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE){
        Serial.print("TX Status Response");
        xbee.getResponse().getZBTxStatusResponse(txStatus);
        if (txStatus.getDeliveryStatus() != SUCCESS) { // This means the far end didn't get it
          Serial.println(F(" failed"));
        }
        Serial.println(" success");
      }
      else {
        Serial.print("Got frame id: ");
        Serial.println(xbee.getResponse().getApiId(), HEX);
      }
    } 
    else if (xbee.getResponse().isError()) {
      // some kind of error happened
      xbee.getResponse().getZBRxResponse(xbrx);
      XBeeAddress64 senderLongAddress = xbrx.getRemoteAddress64();
      Serial.print(F(" XBee error returned: "));
      switch(xbee.getResponse().getErrorCode()){
        case NO_ERROR:
          Serial.print(F("Shouldn't be here, no error "));
          break;
        case CHECKSUM_FAILURE:
          Serial.print(F("Bad Checksum "));
          break;
        case PACKET_EXCEEDS_BYTE_ARRAY_LENGTH:
          Serial.print(F("Packet too big "));
          break;
        case UNEXPECTED_START_BYTE:
          Serial.print(F("Bad start byte "));
          break;
        default:
          Serial.print(F("undefined error"));
          break;
      }
      Serial.println(xbee.getResponse().getErrorCode(),DEC);
    }
    else {
      // This means we don't have a complete packet yet
      pass();
    }
}

void setup() {  
  // start serial
  Serial.begin(9600);
  // and the software serial port
  Serial1.begin(9600);
  pinMode(19, INPUT);  
  digitalWrite(19, HIGH);
  // now that they are started, hook the XBee into 
  // Software Serial
  xbee.setSerial(Serial1);
  // This bypasses the coprocessor on some models
  Serial1.print("B");
  // Now to suck out any characcters that were piled up by the 
  // coprocessor so they don't interfere with XBee transactions
  if (Serial1.available() > 0){
    uint8_t c = Serial1.read();
    if (c < 0x10)
      Serial.print('0');
    Serial.print(c, HEX);
    Serial.print(" ");
  }
  setTime(0); // This is only there to allow the timer to work
  Alarm.timerRepeat(10, sendXBee); // This will cause an XBee message to be sent
  Serial.println("Initialization complete!");
}

void loop() {
  checkXbee();
  Alarm.delay(0); // Just for the alarm routines
}
