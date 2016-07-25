XBeeAddress64 Broadcast = XBeeAddress64(0x00000000, 0x0000ffff);
XBeeAddress64 HouseController= XBeeAddress64(0x0013A200, 0x406f7f8c);

void sendXbee(const char* command){
  ZBTxRequest zbtx = ZBTxRequest(HouseController, (uint8_t *)command, strlen(command));
  xbee.send(zbtx);
}

// this routine is where things are reported if you need it.  Tailor this routine
// any way you want to support your project
char statusBuffer[90];

void pass(){ // This is like the python pass statement. (look it up)
    return;
}

void freezerReport(){
  sprintf(statusBuffer,"Freezer,%lu,%s,%s\r", now(), digitalRead(defrostRelay1Pin) == HIGH ? "ON": "Off",
    dtostrf(readTemp(), 4, 1, t));
  Serial.print(statusBuffer);
  Serial.print("\n");
  
  
  sendXbee(statusBuffer);
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
        //Serial.print("Got frame id: ");
        //Serial.println(xbee.getResponse().getApiId(), HEX);
      }
    } 
    else if (xbee.getResponse().isError()) {
      // some kind of error happened
      xbee.getResponse().getZBRxResponse(xbrx);
      XBeeAddress64 senderLongAddress = xbrx.getRemoteAddress64();
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
      // ending.  This is the case where you haven't quite 
      // completed a packet, just keep accumulating packets
    }
}

void processXbee(char* inbuf, int len){
  time_t temptime = 0;
  char* data;
  char buf[128];  // should be larger than max size of packet

  strncpy(buf, inbuf, min(len, sizeof(buf)));
  buf[min(len, sizeof(buf))] = 0;
  //Serial.println(buf);
  if(!isalpha(*buf) && *buf != '?'){  // the first character is not alpha or '?'
    Serial.println(F("bad starting character, ignoring"));
    return;
  }
  if(strchr(buf, '\r') == 0){ // all packets have to have a cr at the end.
    Serial.println(F("partial packet, ignoring"));
    return;
  }
  //Serial.println(buf);
  if((strstr_P(buf,PSTR("Time"))) != 0){
    //Serial.println(F("Time Data"));
    data = strtok(buf, ",");  // skip to the actual t_time value
    data = strtok(0, ",");   // get the t_time variable
    temptime = atol(data);
    if(abs(temptime - now()) >= 5){  // only if it matters
      Serial.println(F("Setting Time"));
      setTime(temptime);
    }
  }
  else if (strstr_P(buf,PSTR("Pool")) != 0){ 
    //Serial.print(F("Pool Data"));
    pass();
  } 
  else if (strstr_P(buf,PSTR("Power")) != 0){
    //Serial.println(F("Power Data"));
    pass();
  }
  else if (strstr_P(buf,PSTR("Status")) != 0){
    Serial.println(F("House Status Data"));
    pass();
  }
  //
  // Freezer control Strings
  //
  else if (strstr_P(buf,PSTR("Freezer")) != 0){
    if (strstr_P(buf,PSTR("DefrostOn")) != 0){
      if ((digitalRead(defrostRelay1Pin)) == LOW){
        defrosterOn();
      }
    }
    else if (strstr_P(buf,PSTR("DefrostOff")) != 0){
      defrosterOff();
    }
    else if (strstr_P(buf,PSTR("Report")) != 0){
      freezerReport();
      Serial.println(F("Freezer Report Sent"));
    }
    else{
      Serial.print(F("Invalid command to freezer - "));
      Serial.println(buf);
    }
  }
  else if (strstr_P(buf,PSTR("?")) != 0){
    Serial.println(F("Query House Controller"));
    pass();
  }
  else if (strstr_P(buf,PSTR("Garage")) != 0){
    //Serial.println(F("Garage Data"));
    pass();
  }
  else{
    Serial.print(F("Message - "));
    Serial.println(buf);
    pass();
  }
}

