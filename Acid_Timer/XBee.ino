#define WaitForFrameStart 1
#define LengthHighByte 2
#define LengthLowByte 3
#define PayloadCapture 4
#define CheckChecksum 5

uint8_t received;
int framestatus = WaitForFrameStart;
byte checksum;
char* payloadbuffer;
int datalength = 0;
int savedDataLength;

const char statusCommand[] PROGMEM = {
  0x10,0x01,0x00,0x00,0x00,0x00,
  0x00,0x00,0xff,0xff,0xff,0xfe,0x00,0x00,'?','\r'};
const char textPrefix[] PROGMEM = {0x10,// frame type (tx)
        0x01, // data frame number
        0x00,0x13,0xA2,0x00, // first 4 bytes of dest address (sending to house controller)
        0x40,0x55,0xB0,0xA6, // second 4 bytes of dest address
        0xff,0xfe,           // short address of dest (unknown)
        0x00,                // number of hops (max)
        0x00};               // bitfield of options (none chosen)

void sendXbee_P(const char* command, int length){
  byte checksum = 0;
  int i;
  Dbuf[0] = 0x7e;  // begin frame
  Dbuf[1] = length >>8;
  Dbuf[2] = length & 0xff;
  // copy command into buffer calculating checksum at the same time
  for(i = 0; i<length; i++){
    char c = pgm_read_byte(command + i); // can't use command[i]
    Dbuf[i+3] = c;
    checksum += c;
  }
  Dbuf[i+3] = 0xff - checksum;
  Serial.println("Status Request");
  for(i = 0; i<length + 4; i++){
    xbeeSerial.print(Dbuf[i]);
    //    printByteData(Dbuf[i]);
    //    Serial.print(" ");
  }
  //  Serial.println();
  Serial.println("Request Sent");
}

void sendStatusXbee(){
  byte checksum = 0;
  int i, length;
  
  //Just let them know it's alive
  sprintf(Dbuf2,"AcidPump,%lu,%s,%s,%d\r", now(), pumping == true ? "ON": "Off", acidLevel, dayOfYear());
  Serial.print(Dbuf2);
  Serial.print("\n");
  Dbuf[0] = 0x7e;  // begin frame
  //length is sum of prefix and text string
  length = sizeof(textPrefix) + strlen(Dbuf2);
  Dbuf[1] = length >>8;
  Dbuf[2] = length & 0xff;
  //copy in the prefix
  memcpy_P(Dbuf + 3,textPrefix,sizeof(textPrefix)); // This is correct
  for(i=0; i < sizeof(textPrefix); i++){ //this little loop should be removed on next update.
    Dbuf[i + 3] = pgm_read_byte(textPrefix + i);
  }
  //then the text string
  memcpy(Dbuf + 3 + sizeof(textPrefix),Dbuf2,strlen(Dbuf2));

  for(i = 0; i<length; i++){
    checksum += Dbuf[i+3];
  }
  Dbuf[3 + length] = 0xff - checksum;
  for(i = 0; i<length + 4; i++){
    xbeeSerial.print(Dbuf[i]);  // out to XBee
  }
  Serial.println("Alive message sent");
}

void checkXbee(){

  if (xbeeSerial.available()) {
    received = (uint8_t)xbeeSerial.read();
    switch( framestatus ){
    case WaitForFrameStart:
      if(received != 0x7E)
        break;
      else {
        framestatus = LengthHighByte;
        //        Serial.print("Frame start, ");
        checksum = 0;
        payloadbuffer = xbeeIn;
      }
      break;
    case LengthHighByte:
      datalength = received;
      framestatus = LengthLowByte;
      break;
    case LengthLowByte:
      datalength = (datalength * 256) + received;
      savedDataLength = datalength;
      //      Serial.print("length ");
      //      Serial.print(datalength);
      //      Serial.print(", ");
      framestatus = PayloadCapture;
      break;
    case PayloadCapture:
      if(payloadbuffer >= xbeeIn + sizeof(xbeeIn)){
        strcpy_P(Dbuf,PSTR("Reached end of buffer, ignore packet"));
        Serial.println(Dbuf);
        framestatus = WaitForFrameStart;
        break;
      }
      *payloadbuffer++ = received;
      //      printByteData(received);
      //      Serial.print(" ");
      datalength--;
      checksum += received;
      if (datalength == 0){
        framestatus = CheckChecksum;
        *payloadbuffer = '\0';
      }
      break;
    case CheckChecksum:
      checksum += received;
      if (checksum == 0xFF){
        //        Serial.println("Checksum valid.");
        xbeeFrameDecode(xbeeIn, savedDataLength);
      }
      else {
        strcpy_P(Dbuf,PSTR("Checksum Invalid ***********"));
        Serial.println(Dbuf);
      }
      framestatus = WaitForFrameStart;
      break;
    default:
      break;
    }
  }
}

void xbeeFrameDecode(char* buffer, int length){

  switch ( *buffer){
  case 0x90: 
    {
      Serial.println("Receive Data Frame");
      buffer++;                //skip over the frame type
      length--;
      //     Serial.print("Source 64 bit address: ");
      for(int i=0; i<8; i++){  //address the frame came from
        //       printByteData(*buffer);
        //       if (i == 3)
        //         Serial.print(" ");
        buffer++;
        length--;
      }
      //     Serial.println();
      //     Serial.print("Source 16 bit network address: ");
      for(int i=0; i<2; i++){  //16 bit network address the frame came from
        //       printByteData(*buffer);
        buffer++;
        length--;
      }
      //     Serial.println();
      //     Serial.print("Receive options: ");  // options byte
      //     printByteData(*buffer);
      //     Serial.println();
      buffer++;
      length--;
      //     char* tmp = buffer;
      //     while(length-- > 0){ //assuming the actual data is ascii
      //       Serial.print(*tmp++);
      //     }
      processXbee(buffer);
      break;
    }
  case 0x92: 
    {
      Serial.println("Data Sample");
      break;
    }
  case 0x88:
    {
      Serial.println("AT Command Response Frame");
      break;
    }
  case 0x8B: 
    {
      Serial.println("Transmit Status Frame");
      break;
    }
  default: 
    {
      sprintf(Dbuf,"Unimplemented Frame Type %X",(uint8_t)*buffer);
      Serial.println(Dbuf);
      break;
    }
  }
}

void printByteData(uint8_t Byte){
  Serial.print((uint8_t)Byte >> 4, HEX);
  Serial.print((uint8_t)Byte & 0x0f, HEX);
}

void processXbee(char* buf){
  time_t temptime = 0;
  char* data;

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
    data = strtok_r(buf, ",", &buf);  // skip to the actual t_time value
    data = strtok_r(0, ",", &buf),   // get the t_time variable
    temptime = atol(data);
    if(abs(temptime - now()) >= 5){  // only if it matters
      Serial.println("Setting Time");
      setTime(temptime);
    }
  }
  else if (strstr_P(buf,PSTR("Power")) != 0){
    Serial.println("Power Data");
  }
  else if (strstr_P(buf,PSTR("AcidOn")) != 0){
    if ((digitalRead(acidPump) == LOW) && (strcmp(poolData.motorState, "Off") != 0)){
      acidPumpOn();
      Serial.println("Acid Pump On");
    }
    else {
      sendStatusXbee();
    }
  }
  else if (strstr_P(buf,PSTR("AcidOff")) != 0){
    acidPumpOff();
    Serial.println("Acid Pump Off");
  }
  else if (strstr_P(buf,PSTR("AcidStatus")) != 0){
    sendStatusXbee();
    Serial.println("Acid Pump Status Sent");
  }
  else if (strstr_P(buf,PSTR("Pool")) != 0){
    Serial.println("Pool Data");
    data = strtok_r(buf, ",", &buf);   // skip to actual pool data
    buf+=2; //skip the "M "
    char *tb = buf;
    strtok_r(buf, ",", &buf); //location of the next comma
    memset(poolData.motorState,0,sizeof(poolData.motorState));
    strncpy(poolData.motorState,tb,buf-tb);
    // the pointer buf was left at the waterfall
    Serial.print("Motor ");
    Serial.println(poolData.motorState);
    // if the motor is off and the pump is still on, turn the Acid Pump off.
    if((strcmp(poolData.motorState, "Off") == 0) && (digitalRead(acidPump) == HIGH)){
      acidPumpOff();
    }
    buf+=2;
    tb = buf;
    strtok_r(buf, ",", &buf); //location of the next comma
    memset(poolData.waterfallState,0,sizeof(poolData.waterfallState));
    strncpy(poolData.waterfallState,tb,buf-tb);
    // the pointer buf was left at the light
//    Serial.print("Waterfall ");
//    Serial.println(poolData.waterfallState);
    buf+=2;
    tb = buf;
    strtok_r(buf, ",", &buf); //location of the next comma
    memset(poolData.lightState,0,sizeof(poolData.lightState));
    strncpy(poolData.lightState,tb,buf-tb);
    // the pointer buf was left at the fountain
//    Serial.print("Light ");
//    Serial.println(poolData.lightState);
    buf+=2;
    tb = buf;
    strtok_r(buf, ",", &buf); //location of the next comma
    memset(poolData.fountainState,0,sizeof(poolData.fountainState));
    strncpy(poolData.fountainState,tb,buf-tb);
    // the pointer buf was left at the solar valve
//    Serial.print("Fountain ");
//    Serial.println(poolData.fountainState);
    buf+=2;
    tb = buf;
    strtok_r(buf, ",", &buf); //location of the next comma
    memset(poolData.solarState,0,sizeof(poolData.solarState));
    strncpy(poolData.solarState,tb,buf-tb);
    // the pointer buf was left at the water temp
//    Serial.print("Solar ");
//    Serial.println(poolData.solarState);
    buf+=3; //OK, now I'm pointing at the pool temperature
    // however, if the motor is off, the temp is invalid; no water past the sensor.
    int tmp = poolData.poolTemp;
    poolData.poolTemp = atoi(strtok_r(0, ",", &buf)); // so get the temperature and
    // position the pointer correctly for the air temp below
    // but set it to zero if the motor is off
    if(strcmp_P(poolData.motorState, PSTR("Off")) == 0)
      poolData.poolTemp = 10;  // Lowest temp I'm allowing to display
    // if the motor is on and the temp is zero (this happens sometimes ??)
    // keep the last temperature reading
    // That's why I saved the temperature in tmp above
    else if (poolData.poolTemp < 10) // only one digit came back
      poolData.poolTemp = tmp;  //this way it doesn't screw up graphs
//    Serial.print("Pool Temp ");
//    Serial.println(poolData.poolTemp);
    buf+=3;
    poolData.airTemp = atoi(strtok_r(0, ",", &buf));
//    Serial.print("Air Temp ");
//    Serial.println(poolData.airTemp);
  }
  else if (strstr_P(buf,PSTR("Status")) != 0){
    Serial.println("Status Data");
    data = strtok_r(buf, ",", &buf);   // skip 'Status,' to power data
    data = strtok_r(buf, ",", &buf);   // skip power from the controller
    data = strtok_r(buf, ",", &buf);   // skip time field to outside temp data
    data = strtok_r(buf, ",", &buf);  // skip inside temperature
    data = strtok_r(buf, ",", &buf);  // skip outside temperature
    char *tb = buf;
    strtok_r(buf, ",", &buf); //location of the next comma
    memset(poolData.motorState,0,sizeof(poolData.motorState));
    strncpy(poolData.motorState,tb,buf-tb);
    // the pointer buf was left at the waterfall
    Serial.print("Status Message: Motor ");
    Serial.println(poolData.motorState);
    // if the motor is off and the pump is still on, turn the Acid Pump off.
    if((strcmp(poolData.motorState, "Off") == 0) && (digitalRead(acidPump) == HIGH)){
      acidPumpOff();
    }
  }
  else {
    strcpy_P(Dbuf,PSTR("Undefined payload, ignoring"));
    Serial.println(Dbuf);
    //Serial.println(buf);
  }
}

