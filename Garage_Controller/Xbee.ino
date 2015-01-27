XBeeAddress64 Broadcast = XBeeAddress64(0x00000000, 0x0000ffff);
XBeeAddress64 HouseController = XBeeAddress64(0x0013A200, 0x4055B0A6);

void sendXbee(const char* command){
  ZBTxRequest zbtx = ZBTxRequest(HouseController, (uint8_t *)command, strlen(command));
  xbee.send(zbtx);
}

void sendStatusXbee(){
  //Just let them know it's alive
  sprintf(Dbuf2,"Garage,%s,%s,%s\r",
    digitalRead(garageDoor1)==HIGH?"open":"closed",    // status of door 1
    digitalRead(garageDoor2)==HIGH?"open":"closed",    // status of door 2
    digitalRead(waterHeater)==HIGH?"off":"on"          // Waterheater on ??
    );
  Serial.print(Dbuf2);
  Serial.print("\n");  // notice this is only for the serial port
  sendXbee(Dbuf2);     // out to the XBee
  Serial.println("Alive message sent");
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
        processXbee((char *)xbrx.getData(), xbrx.getDataLength());
      }
      else {
        Serial.print("Got frame id: ");
        Serial.println(xbee.getResponse().getApiId(), HEX);
      }
    } 
    else if (xbee.getResponse().isError()) {
      // some kind of error happened, I put the stars in so
      // it could easily be found
      xbee.getResponse().getZBRxResponse(xbrx);
      XBeeAddress64 senderLongAddress = xbrx.getRemoteAddress64();
      print32Bits(senderLongAddress.getLsb());
      Serial.print(F(" XBee error code: "));
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
          Serial.print(F("undefined "));
          break;
      }
      Serial.println(xbee.getResponse().getErrorCode(),DEC);
    }
    else {
      // I hate else statements that don't have some kind
      // ending.  This is where you handle other things
    }
}

void processXbee(char* inbuf, int len){
  time_t temptime = 0;
  char* data;
  char buf[128];  // should be larger than max size of packet

  strncpy(buf, inbuf, min(len, sizeof(buf)));
  buf[min(len, sizeof(buf))] = 0;
  //  Serial.println(buf);
  if(!isalpha(*buf) && *buf != '?'){  // the first character is not alpha or '?'
//    strcpy_P(Dbuf,PSTR("bad starting character, ignoring"));
//    Serial.println(Dbuf);
    return;
  }
  if(strchr(buf, '\r') == 0){ // all packets have to have a cr at the end.
//    strcpy_P(Dbuf,PSTR("partial packet, ignoring"));
//    Serial.println(Dbuf);
    return;
  }
  if((strstr_P(buf,PSTR("Time"))) != 0){
    Serial.println("Time Data");
    data = strtok(buf, ",");  // skip to the actual t_time value
    data = strtok(0, ",");   // get the t_time variable
    temptime = atol(data);
    if(abs(temptime - now()) >= 5){  // only if it matters
      Serial.println("Setting Time");
      setTime(temptime);
    }
  }
  else if (strstr_P(buf,PSTR("Power")) != 0)
    Serial.println("Power Data");
  else if (strstr_P(buf,PSTR("Pool")) != 0)
    Serial.println("Pool Data");
  else if (strstr_P(buf,PSTR("Status")) != 0)
    Serial.println("Status Data");
  else if (strstr_P(buf,PSTR("AcidPump")) != 0)
    Serial.println("Acid Pump");
  else if (strstr_P(buf,PSTR("?")) != 0)
    Serial.println("Query House Controller");
    
  else if (strstr_P(buf,PSTR("Garage")) != 0){
    Serial.println("Garage Command");
    data = strtok(buf, ",");  // skip to the actual command
    data = strtok(0, ",");   // get the command
    if (strstr_P(data,PSTR("door1")) != 0){
      Serial.println(F("Door 1 Command"));
      digitalWrite(garageDoorButton1,LOW);
      Alarm.timerOnce(2,garageDoorButton1Off);
    }
    else if (strstr_P(data,PSTR("door2")) != 0){
      Serial.println(F("Door 2 Command"));
      digitalWrite(garageDoorButton2,LOW);
      Alarm.timerOnce(2,garageDoorButton2Off);
    }
    // water heater power control
    else if (strstr_P(data,PSTR("waterheateron")) != 0){
      Serial.println(F("Water Heater On Command"));
      waterHeaterOn();
    }
    else if (strstr_P(data,PSTR("waterheateroff")) != 0){
      Serial.println(F("Water Heater Off Command"));
      waterHeaterOff();
    }
    else
      Serial.print(F("Invalid Garage Command"));
  }
  else {
    strcpy_P(Dbuf,PSTR("Undefined payload, ignoring"));
    Serial.println(Dbuf);
  }
}
// Utility routines for printing packets when needed for debug
void printByteData(uint8_t Byte){
  Serial.print((uint8_t)Byte >> 4, HEX);
  Serial.print((uint8_t)Byte & 0x0f, HEX);
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

