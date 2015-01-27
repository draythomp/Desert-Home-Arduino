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
      //     Serial.println("Receive Data Frame");
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
      Serial.println("Unimplemented Frame Type");
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
  if((strstr_P(buf,PSTR("Time"))) != 0){
    Serial.println("Time Data");
    digitalWrite(timeLed,HIGH);
    Alarm.timerOnce(1,timeLedOff);
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
    digitalWrite(powerLed,HIGH);
    Alarm.timerOnce(1,powerLedOff);
    data = strtok_r(buf, ",", &buf);   // skip to actual power data
    realPower = atof(strtok_r(0, ",", &buf));
    apparentPower = atof(strtok_r(0, ",", &buf));
    powerFactor = atof(strtok_r(0, ",", &buf));
    rmsVoltage = atof(strtok_r(0, ",", &buf));  //reverse these after fixing monitor !!
    rmsCurrent = atof(strtok_r(0, ",", &buf));
    frequency = atof(strtok_r(0, ",", &buf));
  }
  else if (strstr_P(buf,PSTR("Pool")) != 0){
    Serial.println("Pool Data");
  }
  else if (strstr_P(buf,PSTR("Status")) != 0){
    Serial.println("Status Data");
    digitalWrite(statusLed,HIGH);
    Alarm.timerOnce(1,statusLedOff);
    data = strtok_r(buf, ",", &buf);   // skip 'Status,' to power data
    realPower = atof(strtok_r(buf, ",", &buf));   // grab the power from the controller
    data = strtok_r(buf, ",", &buf);   // skip time field to outside temp data
    insideTemp= atoi(strtok_r(0, ",", &buf));
    outsideTemp= atoi(strtok_r(0, ",", &buf));
  }
}

void houseStatus(){
  sendXbee_P(statusCommand, sizeof(statusCommand));
}

void powerLedOff(){
  digitalWrite(powerLed,LOW);
}
void timeLedOff(){
  digitalWrite(timeLed,LOW);
}
void statusLedOff(){
  digitalWrite(statusLed,LOW);
}

