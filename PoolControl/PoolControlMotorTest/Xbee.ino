XBeeAddress64 Broadcast = XBeeAddress64(0x00000000, 0x0000ffff);
XBeeAddress64 HouseController= XBeeAddress64(0x0013A200, 0x406f7f8c);

void sendXbee(const char* command){
  ZBTxRequest zbtx = ZBTxRequest(HouseController, (uint8_t *)command, strlen(command));
  xbee.send(zbtx);
}

// this routine is where things are reported if you need it.  Tailor this routine
// any way you want to support your project
char statusBuffer[90];

void pass(){
    return;
}

void poolReport(){
  char filterString[10];
  
  if (poolStatusFlag & filterLowSpeedBit)
    strcpy_P(filterString,PSTR("Low"));
  else if ((poolStatusFlag & filterBit) && !(poolStatusFlag & filterLowSpeedBit))
    strcpy_P(filterString,PSTR("High"));
  else
    strcpy_P(filterString,PSTR("Off"));
  sprintf(statusBuffer,"Pool,M %s,W %s,L %s,F %s,S %s,PT %d,AT %d,U %d,I %d\r",
    filterString,
    (poolStatusFlag & waterfallBit ? "On" : "Off"),
    (poolStatusFlag & lightBit ? "On" : "Off"),
    (poolStatusFlag & fountainBit ? "On" : "Off"),
    (poolStatusFlag & solarBit ? "On" : "Off"),
    poolTemp,
    airTemp,
    ++updateCount,
    incomingCount);
    Serial.print(statusBuffer);
    Serial.println();
    sendXbee(statusBuffer);
}
void acidPumpReport(){
  sprintf(statusBuffer,"AcidPump,%lu,%s,%s,%d\r", now(), pumping == true ? "ON": "Off", acidLevel, dayOfYear());
  Serial.print(statusBuffer);
  Serial.print("\n");
  sendXbee(statusBuffer);
}

void septicReport(){
  sprintf(statusBuffer,"Septic,%s\r", septicLevel);
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
      // ending.  This is where you handle other things
      // I get here when a packet needs more than one call to
      // this routine to complete.
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
    //Serial.println(F("Time Data"));
    data = strtok(buf, ",");  // skip to the actual t_time value
    data = strtok(0, ",");   // get the t_time variable
    temptime = atol(data);
    if(abs(temptime - now()) >= 5){  // only if it matters
      Serial.println(F("Setting Time"));
      setTime(temptime);
    }
  }
// this is where incoming pool commands are parsed    
  else if (strstr_P(buf,PSTR("pool")) != 0){ 
    Serial.print(F("Pool Data"));
    Serial.print(F(" command: "));
    char c = buf[5];
    Serial.println(c);
    doPoolCommand(c);
  } 
  else if (strstr_P(buf,PSTR("Power")) != 0)
    //Serial.println(F("Power Data"));
    pass();
  else if (strstr_P(buf,PSTR("Status")) != 0)
    //Serial.println(F("Status Data"));
    pass();
  //
  // Acid Pump control strings
  //
  else if (strstr_P(buf,PSTR("AcidOn")) != 0){
    if ((digitalRead(acidPump) == RELAYOPEN) && (poolStatusFlag & filterBit)){
      acidPumpOn();
    }
    else {
      acidPumpReport();
    }
  }
  else if (strstr_P(buf,PSTR("AcidOff")) != 0){
    acidPumpOff();
  }
  else if (strstr_P(buf,PSTR("AcidStatus")) != 0){
    acidPumpReport();
    Serial.println("Acid Pump Status Sent");
  }
  else if (strstr_P(buf,PSTR("?")) != 0){
    //Serial.println(F("Query House Controller"));
    pass();
}
  else if (strstr_P(buf,PSTR("Garage")) != 0){
    //Serial.println(F("Garage Data"));
    pass();
  }
}

void doPoolCommand(char c){
  newStatusFlag = poolStatusFlag;
  commandPtr = 0;

  switch (c){
    // these are the control commands for waterfall, fountain, and light
    // because of the multispeed motor, it's more complicated
    case '1':
      motorSpeed = motorOff;
      Serial.print("Pool Off");
      return;
      break;
    case '2':
      motorSpeed = motorLow;
      Serial.print("Pool low");
      return;
      break;
    case '3':
      motorSpeed = motorHigh;
       Serial.print("Pool High");
     return;
      break;
    case 'l':
      if ( !(poolStatusFlag & lightBit) ){ //light already off
        poolReport();
        break;
      }
      commandPtr = lightData;
      newStatusFlag &= ~lightBit;  // set flag for off
      break;
    case 'L':
      if (poolStatusFlag & lightBit){ //light already on
        poolReport();
        break;
      }
      commandPtr = lightData;
      newStatusFlag |= lightBit;  // set flag for on
      break;
    case 'w':
      if ( !(poolStatusFlag & waterfallBit) ){ // waterfall already off
        poolReport();
        break;
      }
      commandPtr = waterfallData;
      newStatusFlag &= ~waterfallBit;
      break;
    case 'W':
      if ( poolStatusFlag & waterfallBit ){ // waterfall already on
         poolReport();
         break;
      }
      commandPtr = waterfallData;
      newStatusFlag |= waterfallBit;
      break;
    case 'f':
      if ( !(poolStatusFlag & fountainBit) ){ // fountain already off
         poolReport();
         break;
      }
      commandPtr = fountainData;
      newStatusFlag &= ~fountainBit;
      break;
    case 'F':
      if ( poolStatusFlag & fountainBit ){ // fountain already on
        poolReport();
        break;
      }
      commandPtr = fountainData;
      newStatusFlag |= fountainBit;
      break;
    case 'o':
      if ( !(poolStatusFlag & filterBit)){ // The filter is not running
        poolReport();
        break; // do nothing
      }
      commandPtr = filterData;
      newStatusFlag &= ~filterBit;
      break;
    case 'S':
      if ( (poolStatusFlag & filterBit) && !(poolStatusFlag & filterLowSpeedBit)){ // The filter on high
        poolReport();
        break;  // do nothing
      }
      commandPtr = filterData;
      newStatusFlag |= filterBit; // the filter bit on alone is high speed
      newStatusFlag &= ~filterLowSpeedBit; //so turn off the low speed bit also
      break;
    case 's':
      if ( (poolStatusFlag & filterBit) && (poolStatusFlag & filterLowSpeedBit)){ // The filter on low
        poolReport();
        break;  // do nothing
      }
      commandPtr = filterData;
      newStatusFlag |= (filterBit + filterLowSpeedBit);
      break;
    // these are general purpose commands for debugging
    case 'R':
    case 'r':
      poolReport();
      commandPtr = 0;
      break;
    case 'b':
    case 'B':
      resetFunc();
      break;
    // having trouble telling how long to wait after command to see if it works
    // added these commands to allow the wait period to be experimented with 
    // remotely over the XBee link
    case 'j':  
      commandWait++;
      Serial.print("Wait at: ");
      Serial.println(commandWait);
      break;
    case 'k':
      commandWait--;
      Serial.print("Wait at: ");
      Serial.println(commandWait);
      break;
    default:
      Serial.println("Command error");
      commandPtr = 0;
      break;
    }
  if (commandPtr != 0){ //valid command from XBee envelope
    poolSendFlag();
  }
}

// returns the day of year 1-365 or 366 so Feb 29th is the 60th day.
int dayOfYear(){
  return( 1 + (g(year(), month(), day()) - g(year(), 1, 1)) );
}
// this converts to a day number allowing for leap years and such
int g(int y, int m, int d){
  unsigned long yy, mm, dd;
  dd = d;
  mm = (m + 9) % 12;
  yy = y - mm/10;
  return (int)(365*yy + yy/4 - yy/100 + yy/400 + (mm * 306 + 5)/10 + (dd - 1));
}

