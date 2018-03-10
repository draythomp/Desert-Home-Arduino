//  Put in code to make sure a time update is at least after
//  Jan 1, 2011.  This is after a couple of bad updates that
//  Left the devices (both of them) set for time zero.  Even
//  a reboot left them set for 0.
//
//  Added Button library and support code to make the local
//  button control smoother and easier
//
//  This version has changes for allowing the mode switch on the compressor
//  to stay engaged.  So, it will stay in cool or heat when the compressor is 
//  turned off.
//
// Version 7 has major structural changes to support IDE 1.0.0 and up.  
// The ethernet interface changed in several aspects and things had to be
// done to support it.  As of this date 8/6/2013, this code has NOT been tested
// on a thermostat, it's only been compiled.
//
//
// pin usage:
// removed the 10K pullups from the Arduino ethernet board
// to free up analog pins 0 and 1  (not needed)
// Analog 0 temperature sensor input
// Analog 1 arduino ethernet board reset
// Analog 2 
// Analog 3 pushbutton
// Analog 4 pushbutton
// Analog 5 pushbutton
// Analog 6 pushbutton
// Digital 0 Serial input 
// Digital 1 Serial output
// Digital 2 Arduino ethernet board
// Digital 3
// Digital 4 Serial out to LCD display
// Digital 5 Fan control
// Digital 6 Compressor control
// Digital 7 Heat pump switchover control
// Digital 8 Sense for the ethernet card reset
// Digital 9
// Digital 10 Arduino ethernet board
// Digital 11 Arduino ethernet board
// Digital 12 Arduino ethernet board
// Digital 13 Arduino ethernet board
//
#include <avr/interrupt.h>
#include <avr/io.h>
#include <SPI.h>
#include <Ethernet.h>
#include <SoftwareSerial.h>
#include <Time.h>
#include <EEPROM.h>
#include <MemoryFree.h>
#include <Button.h>
#include "template.h"  //I had to add this because the compiler changed (it's in a tab)

//#define north  // comment this out for South Thermostat
#ifdef north 
#define nameString "North"
#else
#define nameString "South"
#endif

#define tzOffset -7
time_t tNow;
/*   */
// Enter a MAC address and IP address for controller below.
// The IP address will be dependent on your local network:
#ifdef north
byte ip[] = { 192,168,0,202 };
byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
#else
byte ip[] = { 192,168,0,203 };
byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE };
#endif
byte gateway_ip[] = {192,168,0,1};	// router or gateway IP address
byte subnet_mask[] = {255,255,255,0};	// subnet mask for the local network
IPAddress SatClockaddress(192,168,0,204);  //House satellite clock

boolean Clockconnected = false;

// Initialize the Ethernet client library
EthernetClient SatClock;
EthernetServer Control(80);
#define aref_voltage 3.3
#define TempPin 0
#define FanPin 5
#define CompressorPin 6
#define CoolControlPin 7
#define rxSense 8
#define tempupPin A3
Button tempupButton = Button(tempupPin, PULLUP);
#define tempdownPin A2
Button tempdownButton = Button(tempdownPin, PULLUP);
#define fanmodePin A4
Button fanmodeButton = Button(fanmodePin, PULLUP);
#define compressormodePin A5
Button compressormodeButton = Button(compressormodePin, PULLUP);

#define ON 1
#define OFF 0
#define CHECK 5
#define COOLING 55 //mode settings
#define HEATING 56
#define FanRECIRC 57 // three modes for the fan, Recirc, auto and on.  
#define FanON 58     // there is no off to protect the compressor
#define FanAUTO 59

int Cvalue, Fvalue;
char Dbuf[50];  //general purpose buffer for strings
char Dbuf2[50];
char Dbuf3[100];  // this is only for sprintf stuff
int thisHour, thisDay;

struct config_t
{
  long magic_number;
  int tempSetting;
  int modeSetting; //heat or cool
  int fanMode; // recirc, on, auto
  int Hysteresis; //kick in at one degree beyond setting and out at one degree past it
  float TempFudge;
  int tempHigh;
  int tempLow;
} configuration, tempconfig;

int isPeak;
int peakOverride = false;
#define PEAKSTART 0
#define PEAKEND 1
// entries are: peak start hour, peak end hour 
const byte peakArray [7][2] = { {99,0},   //Sunday no peak period
                          {12,20},    //Monday
                          {12,20},    //Tuesday
                          {12,20},    //Wednesday
                          {12,20},    //Thursday
                          {12,20},    //Friday
                          {99,0} }; //Saturday no peak period
                          
// This is so I can reset the board in software if it hang up 
// reading the clock or on the ethernet.
void(* resetFunc) (void) = 0; //declare reset function @ address 0

//
// heat pump routines
//
int Fan(int mode){
  if (mode == ON && digitalRead(FanPin) == LOW){
    digitalWrite(FanPin, HIGH);
//    Serial.println("Fan On");
  }
  if (mode == OFF && digitalRead(FanPin) == HIGH){
    digitalWrite(FanPin, LOW);
//    Serial.println("Fan Off");
  }
  return(digitalRead(FanPin));
}
int Compressor(int mode){
  if (mode == ON && digitalRead(CompressorPin) == LOW){
    digitalWrite(CompressorPin, HIGH);
//    Serial.println("Compressor On");
  }
  else if (mode == OFF && digitalRead(CompressorPin) == HIGH){
    digitalWrite(CompressorPin, LOW);
//    Serial.println("Compressor Off");
  }
  return(digitalRead(CompressorPin));
}
int Cool(int mode){
  if (mode == ON && digitalRead(CoolControlPin) == LOW){
    digitalWrite(CoolControlPin, HIGH);
//    Serial.println("Cool On");
  }
  else if (mode == OFF && digitalRead(CoolControlPin) == HIGH){
    digitalWrite(CoolControlPin, LOW);
//    Serial.println("Heat On");
  }
  return(digitalRead(CoolControlPin));
}
int recirc = 0;
void HandleHeatpump(){
  /* Recirculation (evens the room temperatures)
   if it's 5 minutes after the hour, and the fan is not already running,
   turn it on for 10 minutes.  Do the same thing at 30 minutes after the hour.
   on the second thermostat it'll start at 16 and 41 past the hour for 10 minutes.
   05 - 15 North one on
   16 - 26 South one on
   30 - 40 North one on
   41 - 51 South one on
   I have to check to be sure the system isn't already on cooling or heating.
   Additionally, the two systems have to be controlled to be
   sure both systems aren't recirculating at the same time (Controlling Peak Demand)
   The reason for checking to see if the fan is already on is because the system
   may be in a heating or cooling cycle.
   */
  if (configuration.fanMode ==  FanRECIRC){
#ifdef north
    if ((minute() >= 5) && (minute() <15) && (Fan(CHECK) == OFF)){
#else
    if ((minute() >= 16) && (minute() <26) && (Fan(CHECK) == OFF)){
#endif
      Fan(ON);
      recirc = 1;
    }
    // never turn the fan off if the compressor is on
#ifdef north
    if ((minute() >= 15) && (minute() < 30)&& (Compressor(CHECK) == OFF) && (Fan(CHECK) != OFF)){
#else
    if ((minute() >= 26) && (minute() < 41)&& (Compressor(CHECK) == OFF) && (Fan(CHECK) != OFF)){
#endif
      Fan(OFF);
      recirc = 0;
    }
#ifdef north
    if ((minute() >= 30) && (minute() < 40) && (Fan(CHECK) == OFF)){
#else
    if ((minute() >= 41) && (minute() < 51) && (Fan(CHECK) == OFF)){
#endif
      Fan(ON);
      recirc = 1;
    }
    // minutes work in groups of 60 so I don't have to check the high
#ifdef north
    if ((minute() >= 40) && (Compressor(CHECK) == OFF) && (Fan(CHECK) != OFF)){
#else
    if ((minute() >= 51) && (Compressor(CHECK) == OFF) && (Fan(CHECK) != OFF)){
#endif
      Fan(OFF);
      recirc = 0;
    }
  }
  else if (configuration.fanMode == FanAUTO) {
    // if the fan is running, and not the compressor, turn it off
    if(Fan(CHECK) == ON && Compressor(CHECK) == OFF){
      Fan(OFF);
      recirc = 0;
    }
  }
  else if (configuration.fanMode == FanON){
    Fan(ON);
    recirc = 1;
  }
  else
    Serial.println("Fan control error");
    
  if (isPeak && (peakOverride == false)){
    Compressor(OFF); //No compressor use during peak billing periods.
//    Cool(OFF);
    return;
  }
   
  if (configuration.modeSetting == COOLING){
    if (Fvalue > configuration.tempSetting && Compressor(CHECK) == OFF){
      Fan(ON);
      if(Cool(CHECK) != ON)
        Cool(ON);
      Compressor(ON);
      strcpy_P(Dbuf3,PSTR("%2d:%02d Cooling started at %dF for %dF"));
      sprintf(Dbuf,Dbuf3,hour(),minute(),Fvalue, configuration.tempSetting);
      Serial.println(Dbuf);
    }
    else if (Fvalue <= (configuration.tempSetting - configuration.Hysteresis) && Compressor(CHECK)==ON){
       Compressor(OFF);
//       Cool(OFF);
       if (recirc == 0) 
         Fan(OFF);
       strcpy_P(Dbuf3,PSTR("%2d:%02d Cooling stopped at %dF for %dF"));
       sprintf(Dbuf,Dbuf3,hour(),minute(),Fvalue, configuration.tempSetting);
       Serial.println(Dbuf);
     }
  }
  else if (configuration.modeSetting == HEATING){
    if (Fvalue < configuration.tempSetting && Compressor(CHECK) == OFF){
      Fan(ON);
      if(Cool(CHECK) == ON)
        Cool(OFF);
      Compressor(ON);
      strcpy_P(Dbuf3,PSTR("%2d:%02d Heating started at %dF for %dF"));
      sprintf(Dbuf,Dbuf3,hour(),minute(),Fvalue, configuration.tempSetting);
      Serial.println(Dbuf);
    }
    else if (Fvalue >= (configuration.tempSetting + configuration.Hysteresis) && Compressor(CHECK)==ON){
       Compressor(OFF);
//       Cool(OFF);
       if (recirc == 0)
         Fan(OFF);
       strcpy_P(Dbuf3,PSTR("%2d:%02d Heating stopped at %dF for %dF"));
       sprintf(Dbuf,Dbuf3,hour(),minute(),Fvalue, configuration.tempSetting);
       Serial.println(Dbuf);
     }
  }
  else if (configuration.modeSetting == OFF){
    Compressor(OFF);
    Cool(OFF);
    if (recirc == 0)
      Fan(OFF);
  }
  else
    Serial.println("Compressor mode error");
}

#define InterruptOff  TIMSK2 &= ~(1<<TOIE2)
#define InterruptOn  TIMSK2 |= (1<<TOIE2)
volatile long TempReadCount = 0;
volatile long Temperature = 0;
volatile int int_counter = 0;
volatile int seconds = 0;
int oldSeconds = 0;
unsigned int tcnt2; //used for timer value

// Aruino runs at 16 Mhz, so we have 1000 Overflows per second...
// this little routine will get hit 1000 times a second.
ISR(TIMER2_OVF_vect) {
  long junk;
  int i;
  
  int_counter++;
  if (int_counter == 1000) {
    seconds+=1;
    int_counter = 0;
  }
//  junk = 0;  // oversampling according to Atmel avr121 app note
//  for(i=0; i<16; i++)
//    junk += analogRead(TempPin);  //Oversampling
//  Temperature += (junk >> 4);
  Temperature += analogRead(TempPin);
  TempReadCount ++;  
  TCNT2 = tcnt2;  //reset the timer for next time
}

// Timer setup code borrowed from Sebastian Wallin
// http://popdevelop.com/2010/04/mastering-timer-interrupts-on-the-arduino/
// This was the best explanation I found on the web.

void setupTimer()
{
  //Timer2 Settings:  Timer Prescaler /1024
  // First disable the timer overflow interrupt while we're configuring   
  TIMSK2 &= ~(1<<TOIE2);  
  // Configure timer2 in normal mode (pure counting, no PWM etc.)   
  TCCR2A &= ~((1<<WGM21) | (1<<WGM20));  
  // Select clock source: internal I/O clock 
  ASSR &= ~(1<<AS2);  
  // Disable Compare Match A interrupt enable (only want overflow)
  TIMSK2 &= ~(1<<OCIE2A);  

  // Now configure the prescaler to CPU clock divided by 128   
  TCCR2B |= (1<<CS22)  | (1<<CS20); // Set bits  
  TCCR2B &= ~(1<<CS21);             // Clear bit  

  /* We need to calculate a proper value to load the timer counter. 
   * The following loads the value 131 into the Timer 2 counter register 
   * The math behind this is: 
   * (CPU frequency) / (prescaler value) = 125000 Hz = 8us. 
   * (desired period) / 8us = 125. 
   * MAX(uint8) - 125 = 131; 
   */
  /* Save value globally for later reload in ISR */
  tcnt2 = 131;   

  /* Finally load end enable the timer */
  TCNT2 = tcnt2;  
  TIMSK2 |= (1<<TOIE2);  
  sei();
}
#define LCDrxPin 3
#define LCDtxPin 4
SoftwareSerial LCDSerial = SoftwareSerial(LCDrxPin, LCDtxPin);
//
//  LCD display routines
//


void setupLCD()
{
  pinMode(LCDtxPin, OUTPUT);
  LCDSerial.begin(9600);
  LCDSerial.print("?Bff");   // Intensity to max
  delay(100);
  LCDSerial.print("?c0");    // No Cursor
//  LCDSerial.print("?G216");  // 2x16 display (only have to do this once)
  LCDSerial.print("?f");     //clear command.
  delay(100);

}
void LCDclear()
{
    LCDSerial.print("?f");
}

void LCDline1()
{
  LCDSerial.print("?y0");   //Row 0
  LCDSerial.print("?x00");    //Column 0
}
void LCDline2()
{
  LCDSerial.print("?y1");   //Row 1
  LCDSerial.print("?x00");    //Column 0
}

#define numReadings 20 // these items used for smoothing temp readings
float readings[numReadings];
int index = 0;
float total = 0.0;

void HandleTemp(){ // lots of smoothing in here.  also, uses floats to keep it from jumping
  float T;         // by whole degrees.  
  
  InterruptOff; //so it doesn't change while getting it.
  T = Temperature / TempReadCount;  //get the average of the readings taken during interrupts
  Temperature = 0;                                //and start over for the next second
  TempReadCount = 0;
  InterruptOn;

  total -= readings[index];  // rolling average over 'numReadings'
  readings[index] = T;
  total += T;
  index++;
  if (index >= numReadings)
    index = 0;
  T = total / (float)numReadings;  //average these readings also (smooth it out even more)
  T = T * 3.3 / 1024;
  T = (T - 0.5) * 100.0 + configuration.TempFudge;
//  Serial.print("temp=");
//  Serial.print(T,DEC);
//  Serial.print("C ");
//  Serial.print((T * 9.0)/5.0 + 32.0);
//  Serial.println("F");
  Cvalue = round(T);
  Fvalue = round((T * 9.0)/5.0 + 32.0);
}
int lastF;
char Status[8];

void HandleDisplay(){
//  if ((second() % 60) == 0){  // used in debugging the code
//   sprintf(Dbuf, "Temperature=%dC %dF ",Cvalue,Fvalue);
//   Serial.print(Dbuf);
//   if (isPeak)
//      Serial.println("Peak");
//    else
//      Serial.println("Off Peak");
//  }
  strcpy_P(Status,PSTR("BUG"));
  if ((Compressor(CHECK) == OFF) && (Fan(CHECK) == OFF))
    strcpy_P(Status,PSTR("Idle"));
  if ((Compressor(CHECK) == OFF) && (Fan(CHECK) == ON))
    strcpy_P(Status, PSTR("Recirc"));
  if ((Compressor(CHECK) == ON) && (Cool(CHECK) == OFF))
    strcpy_P(Status, PSTR("Heating"));
  if ((Compressor(CHECK) == ON) && (Cool(CHECK) == ON))
    strcpy_P(Status, PSTR("Cooling"));

//  if (lastF != Fvalue){
//    sprintf(Dbuf,"%d:%2d %d", hour(),minute(),Fvalue - lastF);
//    Serial.println(Dbuf);
//  }
  lastF = Fvalue;
    
  strcpy_P(Dbuf3,PSTR("%3dF %7s  %s"));
  sprintf(Dbuf, Dbuf3,Fvalue,Status, isPeak?"Pk":"  ");

  strcpy_P(Dbuf3,PSTR("%3s %2d:%02d%c %c%c %2d"));
  sprintf(Dbuf2, Dbuf3,dayShortStr(weekday()),hourFormat12(),minute(),(isAM()?'A':'P'),
           (configuration.modeSetting==COOLING?'C':(configuration.modeSetting==HEATING?'H':'O')),
           configuration.fanMode == FanRECIRC?'R':(configuration.fanMode==FanON?'O':'A'),
           configuration.tempSetting); 

  InterruptOff; //LCD display goes nuts if interrupted.
  LCDline1();
  LCDSerial.print(Dbuf);
  LCDline2();
  LCDSerial.print(Dbuf2);
  InterruptOn;
}

boolean trySetTime(){
  char* ptr;
  int cnt, myyear, mymonth, myday, myhour, myminute, mysecond;
  int waitcnt = 0;
  
  ptr = Dbuf;
  while(1){
    if (!Clockconnected){
//      Serial.println("Connecting");
      if (SatClock.connect(SatClockaddress, 80)) {
        Clockconnected = true;
        SatClock.println("GET /time_t HTTP/1.0");
      }
      else{
        LCDSerial.print('*');
        Serial.println("failed");
      }
    }
    if (SatClock.available() > 0){
       while (SatClock.available()) {
         *ptr++ = SatClock.read();
       }
       SatClock.flush(); //suck out any extra chars  
       SatClock.stop();
       while (SatClock.status() != 0){
         delay(5);
       }
       Clockconnected = false;
       
       *ptr='\0';
//       Serial.println(Dbuf); //debug only
       ptr = Dbuf + 44; //time value starts 44 bytes in
       tNow = 0;
       while ( isdigit(*ptr)){
         tNow = tNow * 10 +(*ptr++ - '0');
       }
       //  should be Jan 1, 2011 or later
       if(tNow < 1293840000UL)  // something went wrong with getting the time
         return(false);
       setTime(tNow);
       tNow = now();
       strcpy_P(Dbuf3,PSTR("%02d/%02d/%4d %02d:%02d:%02d "));
       sprintf(Dbuf,Dbuf3,month(tNow),day(tNow),year(tNow),hour(tNow),minute(tNow),second(tNow));
       Serial.print(Dbuf);
       strcpy_P(Dbuf,PSTR("time sync complete"));
       Serial.println(Dbuf);
       return(true);
    }

    if (!Clockconnected){
      SatClock.flush();
      SatClock.stop();
      while (SatClock.status() != 0){
        delay(5);
      }
    } 
    if (waitcnt++ < 10){
//      Serial.println("waiting");  // for debugging
      delay (1000);
    }
    else{
      SatClock.flush();
      SatClock.stop();
      while (SatClock.status() != 0){
        delay(5);
      }
      return(false);
    }
  }
}

time_t SetTime(){
  if(trySetTime() == false)
    resetFunc();  //call reset if you can't get the comm to work
  return(now());
}
  
void eepromadjust(){
  EEPROM_readAnything(0, tempconfig); // get saved settings
  if(configuration.tempSetting != tempconfig.tempSetting ||
    configuration.modeSetting != tempconfig.modeSetting ||
    configuration.fanMode != tempconfig.fanMode ||
    configuration.Hysteresis != tempconfig.Hysteresis ||
    configuration.TempFudge != tempconfig.TempFudge ||
    configuration.tempHigh != tempconfig.tempHigh ||
    configuration.tempLow != tempconfig.tempLow){
      strcpy_P(Dbuf, PSTR("configuration updated"));
      Serial.println(Dbuf);
      EEPROM_writeAnything(0, configuration);
    }
}
void ethernetReset(){
  bool rxState;
  int cnt = 10, retryCount = 10, result;
  
  pinMode(rxSense, INPUT);  //for stabilizing the ethernet board
  pinMode(A1,INPUT);
  
  while(retryCount-- > 0){
    digitalWrite(A1, HIGH);
    pinMode(A1, OUTPUT);
    digitalWrite(A1, LOW);  // ethernet board reset
    delay(100);
    digitalWrite(A1, HIGH);
    delay(2000);
  // now, after the reset, check the rx pin for constant on
    cnt = 10;
    result = 0;
    while (cnt-- != 0){  // simply count the number of times the light is on in the loop
      result += digitalRead(rxSense);
      delay(50);
    }
    Serial.println(result,DEC);
    if (result >=6)      // experimentation gave me this number YMMV
      return;
    delay(50);
    LCDSerial.print('*');
  }
  // OK, I tried 10 times to make it work, just start over.
  resetFunc();
}

void setup() {
  Serial.begin(57600);
  strcpy_P(Dbuf, PSTR("Heat Pump Controls Initialization"));
  Serial.println(Dbuf);
  
  analogReference(EXTERNAL);
  pinMode(FanPin, OUTPUT);
  pinMode(CompressorPin, OUTPUT);
  pinMode(CoolControlPin, OUTPUT);
  Fan(OFF);
  Compressor(OFF);
  Cool(OFF);
  
  // get saved configuration
  EEPROM_readAnything(0, configuration); // get saved settings
  if (configuration.magic_number != 1234){ //this will set it up for very first use
    configuration.magic_number = 1234;
    configuration.tempSetting = 78;
    configuration.modeSetting = COOLING;
    configuration.fanMode = FanRECIRC;
    configuration.Hysteresis = 1;
    configuration.TempFudge = 1.0;
    configuration.tempHigh = 60;
    configuration.tempLow = 60;
    EEPROM_writeAnything(0, configuration);
  }
  strcpy_P(Dbuf3,PSTR("Set for %s at %d"));
  sprintf(Dbuf,Dbuf3,configuration.modeSetting==COOLING?"Cooling":
     (configuration.modeSetting==HEATING?"Heating":"Off"),
     configuration.tempSetting);
  Serial.println(Dbuf);
  strcpy_P(Dbuf3,PSTR("Hysteresis=%d and Temp correction="));
  sprintf(Dbuf,Dbuf3,configuration.Hysteresis);
  Serial.print(Dbuf);
  Serial.println(configuration.TempFudge);
  strcpy_P(Dbuf, PSTR("Initializing LCD Display"));
  Serial.println(Dbuf);
  setupLCD();
  strcpy_P(Dbuf, PSTR("Ethernet ..."));
  LCDline1();
  LCDSerial.print(Dbuf);    //let them know it's alive
  LCDline2();
  // start the Ethernet connection:
  // first, reset the board and make sure it intialized correctly
  ethernetReset();
  
  LCDclear();
  LCDline1();
  strcpy_P(Dbuf, PSTR("Getting Time ..."));
  LCDSerial.print(Dbuf);    //Helps in event of a problem
  LCDline2();
  // now give it an address
  Ethernet.begin(mac, ip, gateway_ip, subnet_mask);
  delay(1000);  // wait a bit for the card to stabilize
  strcpy_P(Dbuf, PSTR("time request"));
  Serial.println(Dbuf);
  setSyncInterval(30 * 60);  //update time every 30 minutes
  setSyncProvider(SetTime); //this will immediately go get the time
  // the SetTime routine will reset the board and start over if it can't
  // sync the time
  
  Control.begin(); // start the web control server
  strcpy_P(Dbuf, PSTR("starting timerinterrupt"));
  Serial.println(Dbuf);
  setupTimer();

}

int firsttime = true;

void loop() {
  int index = 0;

  if(firsttime == true){  // keep running out of memory!!
    Serial.print("Mem=");
    Serial.println(freeMemory());
    firsttime = false;
  }
// Local button handling
  if (tempupButton.uniquePress()){
    Serial.println("Temp up");
    configuration.tempSetting++;
    HandleDisplay();
  }
  if (tempdownButton.uniquePress()){
    Serial.println("Temp down");
    configuration.tempSetting--;
    HandleDisplay();
  }
  if (fanmodeButton.uniquePress()){
    Serial.println("Fan button");
    if (configuration.fanMode == FanRECIRC)
      configuration.fanMode = FanAUTO; 
    else if (configuration.fanMode == FanAUTO)
      configuration.fanMode = FanON;
    else if (configuration.fanMode == FanON)
      configuration.fanMode = FanRECIRC;
    HandleDisplay();
  }
  if (compressormodeButton.uniquePress()){
    Serial.println("Compressor button");
    if (configuration.modeSetting == COOLING){
      configuration.modeSetting = OFF;
    }
    else if (configuration.modeSetting == OFF){
      configuration.modeSetting = HEATING;
    }
    else if (configuration.modeSetting == HEATING){
      configuration.modeSetting = OFF;
      HandleHeatpump();
      configuration.modeSetting = COOLING;
    }
    HandleDisplay();
  }
    
  if (oldSeconds != seconds) {
    if (seconds > 10000)  // integers go to 32K and roll negative, so fix it.
      seconds = 31;       // set it to 31 to prevent temperature redoing average (below)
    oldSeconds = seconds;
    if(second() % 60 == 0 && minute() % 5 == 0)  //check to see if the configuration needs saving
      eepromadjust();                            //every five minutes or so
      
    thisHour = hour();
    thisDay = weekday()-1;
    if (thisHour < (int)peakArray[thisDay][PEAKSTART] || thisHour >= (int)peakArray[thisDay][PEAKEND])
        isPeak = false;
    else
        isPeak = true;
    
    HandleTemp();
    if (thisHour == 0 && peakOverride == true) //just in case it was left overridden
      peakOverride = false;                    //put it back for tomorrow
    if (seconds > 30) // to allow temperature to average out when first started
      HandleHeatpump();
    HandleDisplay();    
    
  }
  //web server control code
   EthernetClient ControlClient = Control.available();
   if (ControlClient) {
    // an http request ends with a blank line
    while (ControlClient.connected()) {
      if (ControlClient.available()) {
        char c = ControlClient.read();
        // if you've gotten to the end of the line (received a newline
        // character) you can send a reply
        if (c != '\n' && c!= '\r') {
          Dbuf2[index++] = c;
          if (index >= sizeof(Dbuf)) // max size of buffer
            index = sizeof(Dbuf) - 1;
          continue;
        }
        Dbuf2[index] = 0;
        Serial.println(Dbuf2);
        // standard http response header
        strcpy_P(Dbuf, PSTR("HTTP/1.1 200 OK"));
        ControlClient.println(Dbuf);
        strcpy_P(Dbuf, PSTR("Content-Type: text/html"));
        ControlClient.println(Dbuf);
        ControlClient.println();
        if (strstr_P(Dbuf2, PSTR("GET / ")) != 0){ // root level request, send status    
          // standard http response header
          strcpy_P(Dbuf, PSTR("<html><body><h1>"));
          ControlClient.print(Dbuf);
          ControlClient.print(nameString);
          strcpy_P(Dbuf, PSTR(" Thermostat<br>"));
          ControlClient.print(Dbuf);
          // output the time
          tNow = now();
          strcpy_P(Dbuf3,PSTR("%02d/%02d/%4d %02d:%02d:%02d <br>"));
          sprintf(Dbuf,Dbuf3,month(tNow),day(tNow),year(tNow),hour(tNow),minute(tNow),second(tNow));
          ControlClient.print(Dbuf);
          strcpy_P(Dbuf3,PSTR("Current Temp: %d <br>"));
          sprintf(Dbuf,Dbuf3,Fvalue);
          ControlClient.print(Dbuf);
          
          strcpy_P(Dbuf3, PSTR("Current Status: %s <br><br>"));
          sprintf(Dbuf, Dbuf3, Status);
          ControlClient.print(Dbuf);
          
          strcpy_P(Dbuf3,PSTR("Settings:<br>Temp: %d<br>Mode: %s<br>Fan Mode: %s<br>Peak Handling: %s<br>Hysteresis: %d<br>"));
          sprintf(Dbuf,Dbuf3,
              configuration.tempSetting, configuration.modeSetting == COOLING?"Cooling":
              (configuration.modeSetting==HEATING?"Heating":"Off"), 
              configuration.fanMode == FanRECIRC?"Recirc":(configuration.fanMode==FanON?"On":"Auto"),
              peakOverride == true ? "OFF" : "ON",
              configuration.Hysteresis);
          ControlClient.print(Dbuf);
          strcpy_P(Dbuf3,PSTR("Temp Correction: %d.%01d<br>"));
          sprintf(Dbuf,Dbuf3,
              (int)configuration.TempFudge,(int)(configuration.TempFudge * 10) % 10);
          ControlClient.print(Dbuf);
          strcpy_P(Dbuf, PSTR("<br></h1></body></head></html>"));
          ControlClient.println(Dbuf);
          break;
        }
        
        else if (strstr_P(Dbuf2,PSTR("GET /+")) != 0){     // up temp by one
          if(configuration.tempSetting < 99){
            configuration.tempSetting++;
          }
          strcpy_P(Dbuf,PSTR("OK"));
          ControlClient.println(Dbuf);
          HandleDisplay();
          break;
       }
        else if (strstr_P(Dbuf2,PSTR("GET /-")) != 0){     // down temp by one
          if (configuration.tempSetting > 40){
            configuration.tempSetting--;
          }
          strcpy_P(Dbuf,PSTR("OK"));
          ControlClient.println(Dbuf);
          HandleDisplay();
          break;
       }
        else if (strstr_P(Dbuf2,PSTR("GET /F")) != 0){     // bump fan one setting
          if (configuration.fanMode == FanRECIRC)
            configuration.fanMode = FanAUTO; 
          else if (configuration.fanMode == FanAUTO)
            configuration.fanMode = FanON;
          else if (configuration.fanMode == FanON)
            configuration.fanMode = FanRECIRC;
          strcpy_P(Dbuf,PSTR("OK"));
          ControlClient.println(Dbuf);
          HandleDisplay();
          break;
       }
        else if (strstr_P(Dbuf2,PSTR("GET /M")) != 0){     // bump mode one setting
          if (configuration.modeSetting == COOLING){
            configuration.modeSetting = OFF;
          }
          else if (configuration.modeSetting == OFF){
            configuration.modeSetting = HEATING;
          }
          else if (configuration.modeSetting == HEATING){
            configuration.modeSetting = OFF;
            HandleHeatpump();
            configuration.modeSetting = COOLING;
          }
          strcpy_P(Dbuf,PSTR("OK"));
          ControlClient.println(Dbuf);
          HandleDisplay();
          break;
        }
   
        else if (strstr_P(Dbuf2,PSTR("GET /status")) != 0){ //for other machines
          strcpy_P(Dbuf3,PSTR("%d,%s,%d,%s,%s,%s"));
          sprintf(Dbuf,Dbuf3,Fvalue,Status,configuration.tempSetting,
          configuration.modeSetting == COOLING?"Cooling":(configuration.modeSetting==HEATING?"Heating":"Off"),
          configuration.fanMode == FanRECIRC?"Recirc":(configuration.fanMode==FanON?"On":"Auto"),
          isPeak?"Peak":"Off Peak"
          );
          ControlClient.println(Dbuf);
          break;
        }
        else if (strstr_P(Dbuf2,PSTR("GET /cool")) != 0){
          configuration.modeSetting = OFF;
          HandleHeatpump();
          configuration.modeSetting = COOLING;
          strcpy_P(Dbuf,PSTR("Web, Mode = Cooling"));
          ControlClient.println(Dbuf);
          break;
        }
        else if (strstr_P(Dbuf2,PSTR("GET /heat")) != 0){
          configuration.modeSetting = OFF;
          HandleHeatpump();
          configuration.modeSetting = HEATING;
          strcpy_P(Dbuf, PSTR("Web, Mode = Heating"));
          ControlClient.println(Dbuf);
          break;
        }
        else if (strstr_P(Dbuf2,PSTR("GET /off")) != 0){
          configuration.modeSetting = OFF;
          HandleHeatpump();
          strcpy_P(Dbuf, PSTR("Web, Mode = Off"));
          ControlClient.println(Dbuf);
          break;
        }
        else if (strstr_P(Dbuf2,PSTR("GET /fan=recirc")) != 0){
          configuration.fanMode = FanRECIRC;
          strcpy_P(Dbuf, PSTR("Web, Fan = Recirc"));
          ControlClient.println(Dbuf);
          break;
        }
        else if (strstr_P(Dbuf2, PSTR("GET /fan=on")) != 0){
          configuration.fanMode = FanON;
          strcpy_P(Dbuf, PSTR("Web, Fan = On"));
          ControlClient.println(Dbuf);
          break;
        }
        else if (strstr_P(Dbuf2, PSTR("GET /fan=auto")) != 0){
          configuration.fanMode = FanAUTO;
          strcpy_P(Dbuf, PSTR("Web, Fan = Auto"));
          ControlClient.println(Dbuf);
          break;
        }
        else if (strstr_P(Dbuf2, PSTR("GET /peakoff")) != 0){
          peakOverride = true;
          strcpy_P(Dbuf, PSTR("Web, Peak handling OFF"));
          ControlClient.println(Dbuf);
          break;
        }
        else if (strstr_P(Dbuf2, PSTR("GET /peakon")) != 0){
          peakOverride = false;
          strcpy_P(Dbuf, PSTR("Web, Peak handling ON"));
          ControlClient.println(Dbuf);
          break;
        }
        else if (strstr_P(Dbuf2,PSTR("GET /hyst+")) != 0){
          configuration.Hysteresis++;
          strcpy_P(Dbuf, PSTR("Web, Hysteresis +"));
          ControlClient.println(Dbuf);
          break;
        }
        else if (strstr_P(Dbuf2,PSTR("GET /hyst-")) != 0){
          configuration.Hysteresis--;
          strcpy_P(Dbuf, PSTR("Web, Hysteresis -"));
          ControlClient.println(Dbuf);
          break;
        }
        else if (strstr_P(Dbuf2,PSTR("GET /temp=")) != 0){
          int tmpI;
          char* tmpS = strtok(Dbuf2,"="); //skip over the first part
          if ((tmpS = strtok(NULL,"=")) != NULL)
            tmpI = atoi(tmpS);
          if (tmpI > 40 && tmpI < 99)
            configuration.tempSetting = tmpI;
          else
            tmpS = NULL;
          if (tmpS == NULL){
            strcpy_P(Dbuf,PSTR("I\'m sorry Dave"));
            ControlClient.println(Dbuf);
          }
          else{
            strcpy_P(Dbuf, PSTR("Web, temp = "));
            Serial.print(Dbuf);
            Serial.println(configuration.tempSetting);
            strcpy_P(Dbuf, PSTR("Temp = "));
            ControlClient.print(Dbuf);
            ControlClient.println(configuration.tempSetting);
          }
          break;
        }
        else if (strstr_P(Dbuf2,PSTR("GET /reset")) != 0){ // remote reset, just for the heck of it
          resetFunc();
          break;  //Won't ever get here 
        }
        else if (strstr_P(Dbuf2,PSTR("GET /save")) != 0){ // forced save to eeprom
          strcpy_P(Dbuf, PSTR("Saving "));
          ControlClient.println(Dbuf);
          eepromadjust();
          break;  
        }
        else if (strstr_P(Dbuf2,PSTR("GET /fudge=")) != 0){
          float tmpD;
          char* tmpS = strtok(Dbuf2,"="); //skip over the first part
          if ((tmpS = strtok(NULL,"=")) != NULL){
            tmpD = atof(tmpS);
          }
          if (tmpD >= -5.0 && tmpD <= 5.0)
            configuration.TempFudge = tmpD ;
          else
            tmpS = NULL;
          if (tmpS == NULL){
            strcpy_P(Dbuf,PSTR("I\'m sorry Dave"));
            ControlClient.println(Dbuf);
          }
          else{
            strcpy_P(Dbuf,PSTR("Web, Fudge = "));
            Serial.print(Dbuf);
            Serial.println(configuration.TempFudge);
            strcpy_P(Dbuf,PSTR("Fudge = "));
            ControlClient.print(Dbuf);
            ControlClient.println(configuration.TempFudge);
          }
          break;
        }
        else{
          strcpy_P(Dbuf,PSTR("command error"));
          ControlClient.println(Dbuf);
          break;
        }
      }
     }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    ControlClient.stop(); 
}


