#define PoolWaitTime 10

// Pool motor commands 
const char poolMotorOffCommand[] = "pool,o\r";
const char poolMotorHighCommand[] = "pool,S\r";
const char poolMotorLowCommand[] = "pool,s\r";
// Pool light commands
const char poolLightOnCommand[] = "pool,L\r";
const char poolLightOffCommand[] = "pool,l\r";
//Pool Fountain commands
const char poolFountainOnCommand[] = "pool,F\r";
const char poolFountainOffCommand[] = "pool,f\r";
// Pool Waterfall commands
const char poolWaterfallOnCommand[] = "pool,W\r";
const char poolWaterfallOffCommand[] = "pool,w\r";
// Pool Controller reset comand
const char poolControllerResetCommand[] = "pool,b\r";

// Acid Pump Commands
const char acidPumpOnCommand[] = "AcidOn\r";
const char acidPumpOffCommand[] = "AcidOff\r";
const char acidPumpStatusCommand[] = "AcidStatus\r";
// Garage commands
const char garageDoorCommand1[] = "Garage,door1\r";
const char garageDoorCommand2[] = "Garage,door2\r";
const char waterHeaterCommandOn[] = "Garage,waterheateron\r";
const char waterHeaterCommandOff[] = "Garage,waterheateroff\r";

XBeeAddress64 Broadcast = XBeeAddress64(0x00000000, 0x0000ffff);

void sendXbee(const char* command){
  ZBTxRequest zbtx = ZBTxRequest(Broadcast, (uint8_t *)command, strlen(command));
  xbee.send(zbtx);
}

void sendStatusXbee(){
  digitalWrite(statusRxLed,LOW);
  Alarm.timerOnce(1,statusRxLedOff);
  // Status message that is broadcast to all devices consists of:
  // power,time_t,outsidetemp,insidetemp,poolmotor  ---more to come someday
  // all fields are ascii with poolmotor being {Low,High,Off}
  sprintf(Dbuf2,"Status,%d,%lu,%d,%d,%s\r",(int)round(realPower), now(),
      (int)round(outsideSensor.temp),
      (ThermoData[0].currentTemp + ThermoData[1].currentTemp)/2,
      poolData.motorState);
  sendXbee(Dbuf2);
}

void checkXbee(){
    // doing the read without a timer makes it non-blocking, so
    // you can do other stuff in loop() as well.
    xbee.readPacket();

    // so the read above will set the available up to 
    // work when you check it.
    if (xbee.getResponse().isAvailable()) {
      // got something
      // I commented out the printing of the entire frame, but
      // left the code in place in case you want to see it for
      // debugging or something.  The actual code is down below.
      //showFrameData();
//      Serial.print("Frame Type is ");
      // Andrew calls the frame type ApiId, it's the first byte
      // of the frame specific data in the packet.
//      Serial.println(xbee.getResponse().getApiId(), HEX);
      
      if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
        // got a zb rx packet, the kind this code is looking for
        
        // now that you know it's a receive packet
        // fill in the values
        xbee.getResponse().getZBRxResponse(xbrx);
        
        // this is how you get the 64 bit address out of
        // the incoming packet so you know which device
        // it came from
//        Serial.print("Got an rx packet from: ");
//        XBeeAddress64 senderLongAddress = xbrx.getRemoteAddress64();
//        print32Bits(senderLongAddress.getMsb());
//        Serial.print(" ");
//        print32Bits(senderLongAddress.getLsb());
        
        // this is how to get the sender's
        // 16 bit address and show it
//        uint16_t senderShortAddress = xbrx.getRemoteAddress16();
//        Serial.print(" (");
//        print16Bits(senderShortAddress);
//        Serial.println(")");
        
        // The option byte is a bit field
//        if (xbrx.getOption() & ZB_PACKET_ACKNOWLEDGED)
            // the sender got an ACK
//          Serial.println("packet acknowledged");
//        if (xbrx.getOption() & ZB_BROADCAST_PACKET)
          // This was a broadcast packet
//          Serial.println("broadcast Packet");
          
//        Serial.print("checksum is ");
//        Serial.println(xbrx.getChecksum(), HEX);
        
        // this is the packet length
//        Serial.print("packet length is ");
//        Serial.print(xbrx.getPacketLength(), DEC);
        
        // this is the payload length, probably
        // what you actually want to use
//        Serial.print(", data payload length is ");
//        Serial.println(xbrx.getDataLength(),DEC);
        
        // this is the actual data you sent
//        Serial.println("Received Data: ");
//        for (int i = 0; i < xbrx.getDataLength(); i++) {
//          print8Bits(xbrx.getData()[i]);
//          Serial.print(' ');
//        }
        
        // and an ascii representation for those of us
        // that send text through the XBee
//        Serial.println();
//        for (int i= 0; i < xbrx.getDataLength(); i++){
//          Serial.write(' ');
//          if (iscntrl(xbrx.getData()[i]))
//            Serial.write(' ');
//          else
//            Serial.write(xbrx.getData()[i]);
//          Serial.write(' ');
//        }
//        Serial.println();
        // Go process the message
        processXbee((char *)xbrx.getData(), xbrx.getDataLength());
//        Serial.println();
      }
      else if (xbee.getResponse().getApiId() == ZB_IO_SAMPLE_RESPONSE) {
        xbee.getResponse().getZBRxIoSampleResponse(ioSample);
//        Serial.print("Received I/O Sample from: ");
        // this is how you get the 64 bit address out of
        // the incoming packet so you know which device
        // it came from
        XBeeAddress64 senderLongAddress = ioSample.getRemoteAddress64();
//        print32Bits(senderLongAddress.getMsb());
//        Serial.print(" ");
//        print32Bits(senderLongAddress.getLsb());
//        Serial.print("addr is ");
//        Serial.println(senderLongAddress.getLsb(),HEX);
        // this is how to get the sender's
        // 16 bit address and show it
        // However, end devices that have sleep enabled
        // will change this value each time they wake up.
//        uint16_t senderShortAddress = ioSample.getRemoteAddress16();
//        Serial.print(" (");
//        print16Bits(senderShortAddress);
//        Serial.println(")");
        // Now, we have to deal with the data pins on the
        // remote XBee
        if (ioSample.containsAnalog()) {
//          Serial.println("Sample contains analog data");
          // the bitmask shows which XBee pins are returning
          // analog data (see XBee documentation for description)
            uint8_t bitmask = ioSample.getAnalogMask();
            for (uint8_t x = 0; x < 8; x++){
            if ((bitmask & (1 << x)) != 0){
//              Serial.print("position ");
//              Serial.print(x, DEC);
//              Serial.print(" value: ");
//              Serial.print(ioSample.getAnalog(x));
//              Serial.println();
              if (senderLongAddress.getLsb() == 0x406f7fac){
                float f = ((((float)ioSample.getAnalog(x) * 1200.0) / 1024.0) / 10.0) * 2.0;
                Serial.print(F("Outside temp is: "));
                Serial.print(f);
                outsideSensor.temp = f;
                outsideSensor.reportTime = now();
                digitalWrite(tempRxLed,LOW);
                Alarm.timerOnce(1,tempRxLedOff);
              }
            }
          }
        }
        // Now, we'll deal with the digital pins
        if (ioSample.containsDigital()) {
          Serial.println("Sample contains digtal data");
          // this bitmask is longer (16 bits) and you have to
          // retrieve it as Msb, Lsb and assemble it to get the
          // relevant pins.
          uint16_t bitmask = ioSample.getDigitalMaskMsb();
          bitmask <<= 8;  //shift the Msb into the proper position
          // and in the Lsb to give a 16 bit mask of pins
          // (once again see the Digi documentation for definition
          bitmask |= ioSample.getDigitalMaskLsb();
          // this loop is just like the one above, but covers all 
          // 16 bits of the digital mask word.  Remember though,
          // not all the positions correspond to a pin on the XBee
          for (uint8_t x = 0; x < 16; x++){
            if ((bitmask & (1 << x)) != 0){
              Serial.print("position ");
              Serial.print(x, DEC);
              Serial.print(" value: ");
              // isDigitalOn takes values from 0-15
              // and returns an On-Off (high-low).
              Serial.print(ioSample.isDigitalOn(x), DEC);
              Serial.println();
            }
          }
        }
        Serial.println();
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
  // print the text string for debugging
  if(strchr(buf,'\n') != 0)  // If the line already has a \n, don't duplicate it.
    Serial.print(buf);
  else
    Serial.println(buf);
  if(!isalpha(*buf) && *buf != '?'){  // the first character is not alpha or '?'
    strcpy_P(Dbuf,PSTR("bad starting character, ignoring"));
    Serial.println(Dbuf);
    return;
  }
  if(strchr(buf, '\r') == 0){ // all packets have to have a cr at the end.
    strcpy_P(Dbuf,PSTR("partial packet, ignoring"));
    Serial.println(Dbuf);
    return;
  }
  if((strstr_P(buf,PSTR("Time"))) != 0){
//    Serial.println("Time Data");
    data = strtok(buf, ",");  // skip to the actual t_time value
    data = strtok(0, ","),   // get the t_time variable
    temptime = atol(data);
    if(abs(temptime - now()) >= 5){  // only if it matters
      //Serial.println("Setting Time");
      setTime(temptime);
    }
    digitalWrite(timeRxLed,LOW);
    Alarm.timerOnce(1,timeRxLedOff);
  }
  else if((strstr_P(buf,PSTR("AcidPump"))) != 0){
    // Acid Pump message is:
    // AcidPump,time_t,status,level,day_of_year
    //
    // the time element is pretty useless since it represents now() on a clock
    // I eventually want it to be the last time the device ran, but that will have
    // to be saved in ROM to allow for reboots and power failures.
    // the day of year is for debugging odd and even days.
    
//    Serial.println("Acid Pump Data");
//    Serial.println(buf);
    acidPumpData.reportTime = now();
    
    data = strtok(buf, ",");  // skip to the actual t_time value
    data = strtok(0, ","),    // get the t_time variable (I'm not using it yet)
    data = strtok(0, ","); //get the state of the pump (on or off)
    memset(acidPumpData.state,0,sizeof(acidPumpData.state));
    strncpy(acidPumpData.state,data,strlen(data));
//    Serial.print("Acid Pump is ");
//    Serial.println(acidPumpData.state);
    // the pointer buf was left at the acid level 
    // which is also the last parameter from this device
    data = strtok(0, ","); //get the state of the pump (OK or LOW)
    memset(acidPumpData.level,0,sizeof(acidPumpData.level));
    strncpy(acidPumpData.level,data,strlen(data));
//    Serial.print("Acid Pump level is ");
//    Serial.println(acidPumpData.level);
    // I'm not going to bother with the day of year value
    // end of acid pump handling
  }
  else if (strstr_P(buf,PSTR("Power")) != 0){
//    Serial.println("Power Data");
    data = strtok(buf, ",");   // skip to actual power data
    realPower = atof(strtok(0, ","));
    apparentPower = atof(strtok(0, ","));
    powerFactor = atof(strtok(0, ","));
    rmsVoltage = atof(strtok(0, ","));  
    rmsCurrent = atof(strtok(0, ","));
    frequency = atof(strtok(0, ","));

//    Serial.println(realPower,2);
//    Serial.println(apparentPower,2);
//    Serial.println(powerFactor,2);
//    Serial.println(rmsCurrent,2);
//    Serial.println(rmsVoltage,2);
//    Serial.println(frequency,2);
    
    digitalWrite(powerRxLed,LOW);
    Alarm.timerOnce(1,powerRxLedOff);
    // end of power monitor handling
  }
  else if (strstr_P(buf,PSTR("Pool")) != 0){
    // Pool data is strange (I know, I did it dumb)
    // it has an indicator of what is being reported in the update message
    // this was a really good idea at the time and it works for debugging so
    // I'll leave it this way.  Causes decoding to be difficult though.
    // Pool,M High,W Off,L Off,F Off,S Off,PT 88,AT 103
    
    // first a little book keeping on the message
    Serial.println("Pool Data");
    digitalWrite(poolRxLed,LOW);
    Alarm.timerOnce(1,poolRxLedOff);
    poolData.reportTime = now();
    
    // now to process the actual message
//    Serial.print(buf);
    data = strtok(buf, ",");   // skip to actual pool data
    data = strtok(0, " ");   // this will skip the M for motor
    data = strtok(0, ","); //location of the comma after motor status
    memset(poolData.motorState,0,sizeof(poolData.motorState));
    strncpy(poolData.motorState,data,strlen(data)); // copy the state
//    Serial.print("Motor ");
//    Serial.println(poolData.motorState);

    // the pointer buf was left at the waterfall
    data = strtok(0, " ");   // this will skip the W for waterfall
    data = strtok(0, ","); // location of the comma after waterfall status
    memset(poolData.waterfallState,0,sizeof(poolData.waterfallState));
    strncpy(poolData.waterfallState,data,strlen(data));
//    Serial.print("Waterfall ");
//    Serial.println(poolData.waterfallState);

    // the pointer buf was left at the light
    data = strtok(0, " ");   // this will skip the L for light
    data = strtok(0, ","); //location of the comma after light status
    memset(poolData.lightState,0,sizeof(poolData.lightState));
    strncpy(poolData.lightState,data,strlen(data));
//    Serial.print("Light ");
//    Serial.println(poolData.lightState);

    // the pointer buf was left at the fountain
    data = strtok(0, " ");   // this will skip the F for fountain
    data = strtok(0, ","); //location of the comma after fountain status
    memset(poolData.fountainState,0,sizeof(poolData.fountainState));
    strncpy(poolData.fountainState,data,strlen(data));
//    Serial.print("Fountain ");
//    Serial.println(poolData.fountainState);

    // the pointer buf was left at the solar valve
    data = strtok(0, " ");   // this will skip the S for solar valve
    data = strtok(0, ","); //location of the comma after solar status
    memset(poolData.solarState,0,sizeof(poolData.solarState));
    strncpy(poolData.solarState,data,strlen(data));
//    Serial.print("Solar ");
//    Serial.println(poolData.solarState);

    // the pointer buf was left at the water temp
    data = strtok(0, " ");   // this will skip the PT for pool temp
    // however, if the motor is off, the temp is invalid; no water past the sensor.
    int tmp = poolData.poolTemp;
    poolData.poolTemp = atoi(strtok(0, ",")); // so get the temperature and
    // position the pointer correctly for the air temp below
    // but set it to something if the motor is off
    if(strcmp_P(poolData.motorState, PSTR("Off")) == 0)
      poolData.poolTemp = 10;  // Lowest temp I'm allowing to display
    // if the motor is on and the temp is zero (this happens sometimes ??)
    // keep the last temperature reading
    // That's why I saved the temperature in tmp above
    else if (poolData.poolTemp < 10) // only one digit came back
      poolData.poolTemp = tmp;  //this way it doesn't screw up graphs
//    Serial.print("Pool Temp ");
//    Serial.println(poolData.poolTemp);

    // the pointer was left at the air temp
    data = strtok(0, " ");   // this will skip the AT for air temp
    poolData.airTemp = atoi(strtok(0, ","));  // grab the air temp
//    Serial.print("Air Temp ");
//    Serial.println(poolData.airTemp);
    // end of pool message processing
  }
  else if (strstr_P(buf,PSTR("?")) != 0){
//    Serial.println("Status Request");
    sendStatusXbee();
  }
  else if (strstr_P(buf,PSTR("Garage")) != 0){
    // Garage message is currently :
    // Garage, door1state,door2state,waterheaterpower/r
    // this will be expanded as devices are added.
//    Serial.println("Garage Report");
    garageData.reportTime = now();
    data = strtok(buf, ",");  // skip to the door report
    data = strtok(0, ",");    // get the status of door 1
    memset(garageData.door1,0,sizeof(garageData.door1));
    strncpy(garageData.door1,data,strlen(data));
//    Serial.print("Garage door 1 is ");
//    Serial.println(garageData.door1);
    
    // now skip to the next door in the data string
    data = strtok(0, ",\r"); //location of the next comma (or end of line)
    memset(garageData.door2,0,sizeof(garageData.door2));
    strncpy(garageData.door2,data,strlen(data));
//    Serial.print("Garage door 2 is ");
//    Serial.println(garageData.door2);
    // now skip to the water heater status
    data = strtok(0,",\r"); //location of the water heater status
    memset(garageData.waterHeater,0,sizeof(garageData.waterHeater));
    strncpy(garageData.waterHeater,data,strlen(data));
//    Serial.print("Water Heater is ");
//    Serial.println(garageData.waterHeater);
  }
  else {
    strcpy_P(Dbuf,PSTR("Message not supported by House Controller"));
    Serial.println(Dbuf);
    Serial.println(buf);
  }
}
// Pool Command send routines to make it easier to set timers and such
void poolMotorOff(){
  if (strcmp_P(poolData.motorState, PSTR("Off")) != 0) {
    sendXbee(poolMotorOffCommand);
    Alarm.timerOnce(PoolWaitTime,poolMotorOff);   // check back in 5 seconds to see if it made it
  }
}
void poolMotorHigh(){
  if (strcmp_P(poolData.motorState, PSTR("High")) != 0) {
    sendXbee(poolMotorHighCommand);
    Alarm.timerOnce(PoolWaitTime,poolMotorHigh);   // check back in 5 seconds to see if it made it
  }
}
void poolMotorLow(){
  if (strcmp_P(poolData.motorState, PSTR("Low")) != 0) {
    sendXbee(poolMotorLowCommand);
    Alarm.timerOnce(PoolWaitTime,poolMotorLow);   // check back in 5 seconds to see if it made it
  }
}
    
void poolWaterfallOff(){
  if (strcmp_P(poolData.waterfallState, PSTR("Off")) != 0) {
    sendXbee(poolWaterfallOffCommand);
    Alarm.timerOnce(PoolWaitTime,poolWaterfallOff);   // check back in 5 seconds to see if it made it
  }
}
void poolWaterfallOn(){
  if (strcmp_P(poolData.waterfallState, PSTR("On")) != 0) {
    sendXbee(poolWaterfallOnCommand);
    Alarm.timerOnce(PoolWaitTime,poolWaterfallOn);   // check back in 5 seconds to see if it made it
  }
}

void poolLightOff(){
  if (strcmp_P(poolData.lightState, PSTR("Off")) != 0) {
    sendXbee(poolLightOffCommand);
    Alarm.timerOnce(PoolWaitTime,poolLightOff);   // check back in 5 seconds to see if it made it
  }
}
void poolLightOn(){
  if (strcmp_P(poolData.lightState, PSTR("On")) != 0) {
    sendXbee(poolLightOnCommand);
    Alarm.timerOnce(PoolWaitTime,poolLightOn);   // check back in 5 seconds to see if it made it
  }
}

void poolFountainOff(){
  if (strcmp_P(poolData.fountainState, PSTR("Off")) != 0) {
    sendXbee(poolFountainOffCommand);
    Alarm.timerOnce(PoolWaitTime,poolFountainOff);   // check back in 5 seconds to see if it made it
  }
}
void poolFountainOn(){
  if (strcmp_P(poolData.fountainState, PSTR("On")) != 0) {
    sendXbee(poolFountainOnCommand);
    Alarm.timerOnce(PoolWaitTime,poolFountainOn);   // check back in 5 seconds to see if it made it
  }
}
void poolControllerReset(){
  Serial.println("Reset to Pool Controller");
  sendXbee(poolControllerResetCommand);
}

void acidPumpOn(){
  sendXbee(acidPumpOnCommand);
}
void acidPumpOff(){
  sendXbee(acidPumpOffCommand);
}
void acidPumpStatus(){
  sendXbee(acidPumpStatusCommand);
  Serial.println("Checking on Acid Pump");
}

void closeGarageDoor1(){
  if (strcmp_P(garageData.door1, PSTR("closed")) != 0) {
    sendXbee(garageDoorCommand1);
  }
}
void closeGarageDoor2(){
  if (strcmp_P(garageData.door2, PSTR("closed")) != 0) {
    sendXbee(garageDoorCommand2);
  }
}
void openGarageDoor1(){
  if (strcmp_P(garageData.door1, PSTR("open")) != 0) {
    sendXbee(garageDoorCommand1);
  }
}
void openGarageDoor2(){
  if (strcmp_P(garageData.door2, PSTR("open")) != 0) {
    sendXbee(garageDoorCommand2);
  }
}
void waterHeaterOn(){
  if (strcmp_P(garageData.waterHeater, PSTR("on")) != 0) {
    sendXbee(waterHeaterCommandOn);
  }
}
void waterHeaterOff(){
  if (strcmp_P(garageData.waterHeater, PSTR("off")) != 0) {
    sendXbee(waterHeaterCommandOff);
  }
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

