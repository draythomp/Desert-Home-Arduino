// Get various parameters from the XBee
AtCommandRequest atRequest = AtCommandRequest();
AtCommandResponse atResponse = AtCommandResponse();

void getDeviceParameters(){
  uint32_t addrl = 0;
  uint32_t addrh = 0;
  uint32_t daddrl = 0;
  uint32_t daddrh = 0;
  
  if(sendAtCommand((uint8_t *)"VR")){
    if (atResponse.getValueLength() != 2)
      Serial.println(F("Wrong length in VR response"));
    if (atResponse.getValue()[1] != 0xA0)
      Serial.println(F("Double check the XBee firmware version"));
  }
  if(sendAtCommand((uint8_t *)"AP")){
    if (atResponse.getValue()[0] != 0x02)
      Serial.println(F("Double Check the XBee AP setting, should be 2"));
  }
  if(sendAtCommand((uint8_t *)"AO")){
    if (atResponse.getValue()[0] != 0)
      Serial.println(F("Double Check the XBee A0 setting, should be 0"));
  }
  
  if(sendAtCommand((uint8_t *)"NI")){
    memset(deviceName, 0, sizeof(deviceName));
    for (int i = 0; i < atResponse.getValueLength(); i++) {
      deviceName[i] = (atResponse.getValue()[i]);
    }
    if (atResponse.getValueLength() == 0){
      Serial.println(F("Couldn't find a device name"));
    }
  }
  if(sendAtCommand((uint8_t *)"SH")){
    for (int i = 0; i < atResponse.getValueLength(); i++) {
      addrh = (addrh << 8) + atResponse.getValue()[i];
    }
  }
  if(sendAtCommand((uint8_t *)"SL")){
    for (int i = 0; i < atResponse.getValueLength(); i++) {
      addrl = (addrl << 8) + atResponse.getValue()[i];
    }
  }
  ThisDevice=XBeeAddress64(addrh,addrl);
  if(sendAtCommand((uint8_t *)"DH")){
    for (int i = 0; i < atResponse.getValueLength(); i++) {
      daddrh = (daddrh << 8) + atResponse.getValue()[i];
    }
  }
  if(sendAtCommand((uint8_t *)"DL")){
    for (int i = 0; i < atResponse.getValueLength(); i++) {
      daddrl = (daddrl << 8) + atResponse.getValue()[i];
    }
  }
  Destination=XBeeAddress64(daddrh,daddrl);
}

uint8_t frameID = 12;

boolean sendAtCommand(uint8_t *command) {
  while(1){
    frameID++;
    atRequest.setFrameId(frameID);
    atRequest.setCommand(command);
    // send the command
    xbee.send(atRequest);
    //Serial.println("sent command");
    // now wait up to 5 seconds for the status response
    if (xbee.readPacket(5000)) {
      // got a response!
  
      // should be an AT command response
      if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE) {
        xbee.getResponse().getAtCommandResponse(atResponse);
  
        if (atResponse.isOk()) {
          //Serial.println("response OK");
          if (atResponse.getFrameId() == frameID){
            //Serial.print("Frame ID matched: ");
            //Serial.println(atResponse.getFrameId());
            return(true);
          }
          else {
            Serial.println("Frame Id did not match");
          }
        }
        else {
          Serial.print(F("Command returned error code: "));
          Serial.println(atResponse.getStatus(), HEX);
        }
      }
      else {
        //Serial.print(F("Expected AT response but got "));
        //Serial.println(xbee.getResponse().getApiId(), HEX);
      }
    } 
    else {
      // at command failed
      if (xbee.getResponse().isError()) {
        Serial.print(F("Error reading packet.  Error code: "));  
        Serial.println(xbee.getResponse().getErrorCode());
        return(false);
      } 
      else {
        Serial.println(F("No response from radio"));  
      }
    }
  }
}

void sendXbee(const char* command){
  ZBTxRequest zbtx = ZBTxRequest(Destination, (uint8_t *)command, strlen(command));
  xbee.send(zbtx);
}

// this routine is where things are reported if you need it.  Tailor this routine
// any way you want to support your project
char statusBuffer[90];

void pass(){ // This is like the python pass statement. (look it up)
    return;
}

void Report(){
  sprintf(statusBuffer,"%s,%lu,%s\r", deviceName, now(), dtostrf(readTemp(), 4, 1, t));
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
  // control Strings
  //
  else if (strstr_P(buf,deviceName) != 0){
    if (strstr_P(buf,PSTR("Report")) != 0){
      Report();
      Serial.println(F("Report Sent"));
    }
    else{
      Serial.print(F("Invalid command - "));
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

