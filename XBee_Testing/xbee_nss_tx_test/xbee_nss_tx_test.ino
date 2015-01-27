/**
 * Copyright (c) 2009 Andrew Rapp. All rights reserved.
 *
 * This file is part of XBee-Arduino.
 *
 * XBee-Arduino is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * XBee-Arduino is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with XBee-Arduino.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <XBee.h>
#include <SoftwareSerial.h>

/*
This example is for Series 2 XBee
 Sends a ZB TX request with the value of analogRead(pin5) and checks the status response for success
*/

XBee xbee = XBee();

char payload[] = {"Hello World\r"};

// SH + SL Address of receiving XBee
//XBeeAddress64 addr64 = XBeeAddress64(0x0013a200, 0x406F7F8C);
XBeeAddress64 addr64 = XBeeAddress64(0x00000000, 0x0000ffff);

ZBTxRequest zbTx = ZBTxRequest(addr64, (uint8_t *)payload, sizeof(payload));
ZBTxStatusResponse txStatus = ZBTxStatusResponse();

#define ssRX 2
#define ssTX 3
SoftwareSerial nss(ssRX, ssTX);

void setup() {
  Serial.begin(9600);
  nss.begin(9600);
  xbee.setSerial(nss);
  
  Serial.println("*** Starting ***");
}

void loop() {   

  xbee.send(zbTx);
  Serial.println("it went out, go for response");
  // after sending a tx request, we expect a status response
  // wait up to half second for the status response
  if (xbee.readPacket(1000)) {
    // got a response!

    // should be a znet tx status            	
    if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      xbee.getResponse().getZBTxStatusResponse(txStatus);

      // get the delivery status, the fifth byte
      if (txStatus.getDeliveryStatus() == SUCCESS) {
        // success.  time to celebrate
        Serial.println("got a good response to transmit");
        for (int i = 0; i<2; i++){
          Serial.print(txStatus.getFrameData()[i+1], HEX);
          Serial.print(" ");
        }
        Serial.println();
      }
      else {
        // the remote XBee did not receive our packet. is it powered on?
        Serial.println("some kind of error in response");
      }
    }
  } 
  else if (xbee.getResponse().isError()) {
      Serial.println("some other kind of error");
  } 
  else {
    // local XBee did not provide a timely TX Status Response -- should not happen
    Serial.println("didn't get a response in half second");
  }

  delay(5000);
}

