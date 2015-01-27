char lcdBuf[17];  // to handle the various text items
//
//  Sparkfun LCD display routines
//
void lcdInit(){
  lcdDisplay.begin(9600);
  lcdDisplay.write(0x7C);   // Intensity to max
  lcdDisplay.write(157);
  delay(100);
  lcdClear();
}
void lcdClear(){
  lcdDisplay.write(0xFE);   //command flag
  lcdDisplay.write(0x01);   //clear command.
  delay(100);
}
void lcdGoto(int position) { //position = line 1: 0-15, line 2: 16-31, 31+ defaults back to 0
  if (position<16)
  { 
    lcdDisplay.write(0xFE);              //command flag
    lcdDisplay.write(position+128);    //position
  }
  else if (position<32){
    lcdDisplay.write(0xFE);   //command flag
    lcdDisplay.write(position+48+128);          //position 
  } 
  else 
  {
    lcdGoto(0); 
  }
}
void lcdLine1()
{
  lcdDisplay.write(0xFE);   //command flag
  lcdDisplay.write(128);    //position to line 1
}
void lcdLine2()
{
  lcdDisplay.write(0xFE);   //command flag
  lcdDisplay.write(192);    //position to line 2
}
void lcdCenterPrint(char *buf){
  char tbuf[17];
  memset(tbuf,0,sizeof(tbuf));
  int length = strlen(buf);
  int i;
  for (i = 0; i <= (16-length)/2-1; i++){ 
    tbuf[i]=' ';
  }
  strncpy(tbuf+i,buf,min(strlen(buf),sizeof(tbuf)-i));
  lcdDisplay.print(tbuf);
}
void lcdLine1Print(char *buf){
  lcdLine1();
  lcdDisplay.print("                ");
  lcdLine1();
  lcdCenterPrint(buf);
}

void lcdLine2Print(char *buf){
  lcdLine2();
  lcdDisplay.print("                ");
  lcdLine2();
  lcdCenterPrint(buf);
}


#define showTime 1
#define showDate 2
#define showPower 3
#define showGarageDoors 4
#define showInsideTemp 5
#define showOutsideTemp 6
#define showPoolTemp 7
#define acidPumpReport 8
#define showWaterHeater 9
#define showCosmUpdateTime 10
#define showEmonUpdateTime 11
#define showThingspeakUpdateTime 12
#define showMemLeft 13
int displayState = showTime;

void rotatingDisplay(){
  int poolMotorState = 0;
  
  problemCheck();  // routine to see if there is something abnormal happening
  
  switch (displayState){
    case showTime:
      displayState = showDate;
      lcdLine1Print("Time");
      strcpy_P(Dbuf2,PSTR("%3s %2d:%02d%c"));
      sprintf(lcdBuf, Dbuf2, dayShortStr(weekday()),hourFormat12(),minute(),(isAM()?'A':'P'));
      lcdLine2Print(lcdBuf);
//      while(1){}; //interrupt test
      break;
    case showDate:
      displayState = showPower;
      strcpy_P(Dbuf2,PSTR("Date %d"));
      sprintf(lcdBuf,Dbuf2,dayOfYear());
      lcdLine1Print(lcdBuf);
      strcpy_P(Dbuf2,PSTR("%d/%d/%d"));
      sprintf(lcdBuf, Dbuf2, month(), day(), year());
      lcdLine2Print(lcdBuf);
      break;
    case showPower:
      displayState = showGarageDoors;
      lcdLine1Print("Power Usage");
      strcpy_P(Dbuf2,PSTR("%dW"));
      sprintf(lcdBuf, Dbuf2, (int)round(realPower));
      lcdLine2Print(lcdBuf);
      break;
    case showGarageDoors:
      displayState = showInsideTemp;
      lcdLine1Print("Garage Doors");
      if(abs(garageData.reportTime - now()) >= (5*60)){  // has it been 5 minutes since last report?
        strcpy_P(lcdBuf,PSTR("No Report"));
        lcdLine2Print(lcdBuf);
        break;
      }
      strcpy_P(Dbuf2,PSTR("%s, %s"));
      sprintf(lcdBuf, Dbuf2, garageData.door1, garageData.door2);
      lcdLine2Print(lcdBuf);
      break;
    case showInsideTemp:
      displayState = showOutsideTemp;
      lcdLine1Print("Inside Temp");
      if(abs(ThermoData[0].reportTime - now()) >= (15*60)){  // has it been 15 minutes since last report?
        strcpy_P(lcdBuf,PSTR("No Report"));
        lcdLine2Print(lcdBuf);
        break;
      }
      if(abs(ThermoData[1].reportTime - now()) >= (15*60)){  // has it been 15 minutes since last report?
        strcpy_P(lcdBuf,PSTR("No Report"));
        lcdLine2Print(lcdBuf);
        break;
      }
      strcpy_P(Dbuf2,PSTR("%dF"));
      sprintf(lcdBuf, Dbuf2, (ThermoData[0].currentTemp + ThermoData[1].currentTemp)/2);
      lcdLine2Print(lcdBuf);
      break;
    case showOutsideTemp:
      displayState = showPoolTemp;
      lcdLine1Print("Outside Temp");
      if(abs(outsideSensor.reportTime - now()) >= (15*60)){  // has it been 15 minutes since last report?
        strcpy_P(lcdBuf,PSTR("No Report"));
        lcdLine2Print(lcdBuf);
        break;
      }
      strcpy_P(Dbuf2,PSTR("%dF"));
      sprintf(lcdBuf, Dbuf2, (int)round(outsideSensor.temp));
      lcdLine2Print(lcdBuf);
      break;
    case showPoolTemp:
      displayState = acidPumpReport;
      lcdLine1Print("Pool Temp");
      if(abs(poolData.reportTime - now()) >= (15*60)){  // has it been 15 minutes since last report?
        strcpy_P(lcdBuf,PSTR("No Report"));
        lcdLine2Print(lcdBuf);
        acidPumpStatus(); // no report in a while, wake up the conversation
        break;
      }
      if(strcmp(poolData.motorState,"High") == 0)
         poolMotorState = 2;
      else if(strcmp(poolData.motorState,"Low") == 0)
        poolMotorState = 1;
      if(poolMotorState == 0){
        strcpy_P(lcdBuf,PSTR("Motor Off"));
        lcdLine2Print(lcdBuf);
      }
      else{
        strcpy_P(Dbuf2,PSTR("%dF"));
        sprintf(lcdBuf, Dbuf2, poolData.poolTemp);
        lcdLine2Print(lcdBuf);
      }
      break;
    case acidPumpReport:
      displayState = showWaterHeater;
      lcdLine1Print("Acid Pump");
      if(abs(acidPumpData.reportTime - now()) >= (15*60)){  // has it been 15 minutes since last report?
        strcpy_P(lcdBuf,PSTR("No Report"));
        lcdLine2Print(lcdBuf);
        acidPumpStatus();  // no report in a while, wake up the conversation.
        break;
      }
      strcpy_P(Dbuf2,PSTR("%s  %s"));
      sprintf(lcdBuf, Dbuf2, acidPumpData.state, acidPumpData.level);
      lcdLine2Print(lcdBuf);
      break;
    case showWaterHeater:
      displayState = showCosmUpdateTime;
      lcdLine1Print("Water Heater");
      if(abs(garageData.reportTime - now()) >= (15*60)){  // has it been 15 minutes since last report?
        strcpy_P(lcdBuf,PSTR("No Report"));
        lcdLine2Print(lcdBuf);
        break;
      }
      strcpy_P(Dbuf2,PSTR("is %s"));
      sprintf(lcdBuf, Dbuf2, garageData.waterHeater);
      lcdLine2Print(lcdBuf);
      break;
    case showCosmUpdateTime:
      displayState = showEmonUpdateTime;
      lcdLine1Print("Cosm Update Time");
      if(abs(pachubeUpdateTime - now()) >= (15*60)){  // has it been 15 minutes since last report?
        strcpy_P(lcdBuf,PSTR("No Report"));
        lcdLine2Print(lcdBuf);
        break;
      }
      strcpy_P(Dbuf2,PSTR("%2d:%02d"));
      sprintf(lcdBuf, Dbuf2, hour(pachubeUpdateTime), minute(pachubeUpdateTime));
      lcdLine2Print(lcdBuf);
      break;
    case showEmonUpdateTime:
      displayState = showThingspeakUpdateTime;
      lcdLine1Print("emon Update Time");
      if(abs(emoncmsUpdateTime - now()) >= (15*60)){  // has it been 15 minutes since last report?
        strcpy_P(lcdBuf,PSTR("No Report"));
        lcdLine2Print(lcdBuf);
        break;
      }
      strcpy_P(Dbuf2,PSTR("%2d:%02d"));
      sprintf(lcdBuf, Dbuf2, hour(emoncmsUpdateTime), minute(emoncmsUpdateTime));
      lcdLine2Print(lcdBuf);
      break;
    case showThingspeakUpdateTime:
      displayState = showMemLeft;
      lcdLine1Print("ts Update Time");
      if(abs(thingspeakUpdateTime - now()) >= (15*60)){  // has it been 15 minutes since last report?
        strcpy_P(lcdBuf,PSTR("No Report"));
        lcdLine2Print(lcdBuf);
        break;
      }
      strcpy_P(Dbuf2,PSTR("%2d:%02d"));
      sprintf(lcdBuf, Dbuf2, hour(thingspeakUpdateTime), minute(thingspeakUpdateTime));
      lcdLine2Print(lcdBuf);
      break;
    case showMemLeft:
      displayState = showTime;
      strcpy_P(Dbuf2,PSTR("Timers: %d of %d"));
      sprintf(lcdBuf, Dbuf2, maxAlarms,dtNBR_ALARMS );
      lcdLine1Print      (lcdBuf);
      strcpy_P(Dbuf2,PSTR("Mem Left: %d"));
      sprintf(lcdBuf, Dbuf2, freeMemory());
      lcdLine2Print(lcdBuf);
      break;
    
    default:
      displayState = showTime;
      lcdLine1Print("OOPS!");
      lcdLine2Print("BUG");
      break;
  }
  Alarm.timerOnce(3,rotatingDisplay);
}

void problemCheck(){
  if(
      // Problems related to the Acid Pump
      (strcmp_P(acidPumpData.state, PSTR("ON")) == 0) ||       // Acid Pump is on (information)
      (strcmp_P(acidPumpData.level, PSTR("LOW")) == 0) ||      // Acid Pump is low
      (abs(acidPumpData.reportTime - now()) >= (15*60)) ||     // Acid Pump hasn't reported
      // Problems related to the Pool
      (abs(poolData.reportTime - now()) >= (15*60)) ||         // Pool hasn't reported
      // Problems with data upload to services
      (abs(thingspeakUpdateTime - now()) >= (15*60)) ||        // These things haven't reported
      (abs(emoncmsUpdateTime - now()) >= (15*60)) ||
      (abs(pachubeUpdateTime - now()) >= (15*60)) ||
      // Problems with garage door
      (abs(garageData.reportTime - now()) >= (5*60)) ||
      (strcmp_P(garageData.door1, PSTR("open")) == 0) ||
      (strcmp_P(garageData.door2, PSTR("open")) == 0) ||
      // Outside temperature sensor
      (abs(outsideSensor.reportTime - now()) >= (15*60)) ||
            
      // this is always false, just to show end of if statement
      ( 1 == 0))
     {
       digitalWrite(lookForProblemLed,LOW);
     }
     else{
       digitalWrite(lookForProblemLed,HIGH);
     }
}

// The routine below is totally useless.
// I just thought it would be cool to flash the lignts in sequence
// purely for fun.
// I guess it does show that the leds work OK though
void initLeds(){
  int i;
  
  for(i=24; i<32; i++){
    pinMode(i,OUTPUT);
    digitalWrite(i,LOW);
  }
  pinMode(39,OUTPUT);
  digitalWrite(39,LOW);
  delay(250);
    
  pinMode(nRecircLed,OUTPUT);
  digitalWrite(nRecircLed,LOW);
  pinMode(sRecircLed,OUTPUT);
  digitalWrite(sRecircLed,LOW);
  delay(1000);
  digitalWrite(nRecircLed,HIGH);
  digitalWrite(sRecircLed,HIGH);
  
  pinMode(nCoolLed,OUTPUT);
  digitalWrite(nCoolLed,LOW);
  pinMode(sCoolLed,OUTPUT);
  digitalWrite(sCoolLed,LOW);
  delay(500);
  digitalWrite(nCoolLed,HIGH);
  digitalWrite(sCoolLed,HIGH);
  
  pinMode(nHeatLed,OUTPUT);
  digitalWrite(nHeatLed,LOW);
  pinMode(sHeatLed,OUTPUT);
  digitalWrite(sHeatLed,LOW);
  delay(500);
  digitalWrite(nHeatLed,HIGH);
  digitalWrite(sHeatLed,HIGH);
  delay(250);
  
  digitalWrite(powerRxLed, HIGH);
  delay(250);
  digitalWrite(timeRxLed, HIGH);
  delay(250);
  digitalWrite(tempRxLed, HIGH);
  delay(250);
  digitalWrite(etherRxLed, HIGH);
  delay(250);
  digitalWrite(extEtherRxLed, HIGH);
  delay(250);
  digitalWrite(securityOffLed, HIGH);
  delay(250);
  digitalWrite(statusRxLed, HIGH);
  delay(250);
  digitalWrite(poolRxLed, HIGH);
  //delay(250);
  //digitalWrite(lookForProblemLed, HIGH);
 }
