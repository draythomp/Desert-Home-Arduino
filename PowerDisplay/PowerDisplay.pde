/*
 * Sketch to grab data from my power monitor and display real
 * power and satellite derived time on a serial enabled LCD.
 * Credits:
 * The idea for the color display was totally stolen from Alan Meany 
 * http://www.alanmeany.net/ambientknowledge.html  I'm sure I didn't use the same techniques as he did,
 * but the idea came from there.
 *
 * Open source rules!
 */
// Version 5, I changed the XBee parsing to get the power from the house
// controller.  So, the power monitor sends it to the house controller and 
// it passes it around the house in the status message it sends.  I did this
// as part of the overall clean up of the XBee network.
//
// This version has changes for using map() to handle the indicator led
// the indicator led was confusing because it works from 255-off to 0-full on.
// And, the green and red work backwards to each other to change color from green
// (low usage) to red (high usage).  Additionally,the green led grossly 
// overpowers the red one so it was really confusing to decide how to handle 
// this.  Especially when I want to dim it when the room lights are out.  
// Otherwise, it looks like a color changing searchlight.
//
// I also moved the XBee SoftwareSerial port to stop conflict with
// pwm pins

#include <Time.h>
#include <TimeAlarms.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

time_t tNow;
float realPower,apparentPower,powerFactor,rmsCurrent,rmsVoltage,frequency;
int insideTemp, outsideTemp;

char Dbuf[50];
char Dbuf2[50];
char xbeeIn[100]; // accumulated XBee input

#define LCDrxPin 3
#define LCDtxPin 4
SoftwareSerial lcdSerial = SoftwareSerial(LCDrxPin, LCDtxPin);
#define xbeeRxPin 2
#define xbeeTxPin 12
SoftwareSerial xbeeSerial = SoftwareSerial(xbeeRxPin, xbeeTxPin);
#define powerLed 7
#define timeLed 8
#define statusLed 9 

void showMem(){
  uint8_t * heapptr, * stackptr;
  
  strcpy_P(Dbuf,PSTR("Mem = "));
  Serial.print(Dbuf);
  stackptr = (uint8_t *)malloc(4);   // use stackptr temporarily
  heapptr = stackptr;                // save value of heap pointer
  free(stackptr);                    // free up the memory again (sets stackptr to 0)
  stackptr =  (uint8_t *)(SP);       // save value of stack pointer
  Serial.println(stackptr - heapptr);
}

//
//  Sparkfun LCD display routines
//
void LCDgoto(int position) { //position = line 1: 0-15, line 2: 16-31, 31+ defaults back to 0
  if (position<16)
  { 
    lcdSerial.print(0xFE, BYTE);              //command flag
    lcdSerial.print((position+128), BYTE);    //position
  }
  else if (position<32){
    lcdSerial.print(0xFE, BYTE);   //command flag
    lcdSerial.print((position+48+128), BYTE);          //position 
  } 
  else 
  {
    LCDgoto(0); 
  }
}

void LCDline1()
{
  lcdSerial.print(0xFE, BYTE);   //command flag
  lcdSerial.print(128, BYTE);    //position to line 1
}

void LCDline2()
{
  lcdSerial.print(0xFE, BYTE);   //command flag
  lcdSerial.print(192, BYTE);    //position to line 2
}

void showPower(){
  
  LCDline1();
  strcpy_P(Dbuf, PSTR("                "));
  lcdSerial.print(Dbuf); //sixteen spaces to clear line
  LCDline1();
  strcpy_P(Dbuf2,PSTR("%dW %dF %dF"));
  sprintf(Dbuf,Dbuf2,(int)round(realPower), outsideTemp, insideTemp);
  lcdSerial.print(Dbuf);         //Actual power usage gets displayed here.
  colorLamp((int)round(realPower));              //set the light color
}

// Displays the time on the LCD screen
void showTime(){
  
  tNow = now();
  LCDline2();
  strcpy_P(Dbuf, PSTR("                "));  //clear out line 2
  lcdSerial.print(Dbuf);
  strcpy_P(Dbuf2,PSTR("%2d:%02d %d/%d/%02d"));
  sprintf(Dbuf,Dbuf2,hour(tNow),minute(tNow),month(tNow),day(tNow),year(tNow)-2000);
  LCDline2();
  lcdSerial.print(Dbuf);
}

// Indicator light handling 
// Not using the Blue LED
#define greenPin 3      // LED connected to these pins
#define redPin 6
#define OFFPEAKMAX 0
#define PEAKMAX 1
#define PEAKSTART 2
#define PEAKEND 3
// entries are: off peak max, peak max, peak start hour, peak end hour
prog_int32_t pwrRangeArray [7][4] PROGMEM = {
  {
    10000,10000,12,19    }
  ,   //Sunday no peak period
  {
    10000,2000,12,19    }
  ,    //Monday
  {
    10000,1350,12,19    }
  ,    //Tuesday
  {
    10000,2000,12,19    }
  ,    //Wednesday
  {
    10000,2000,12,19    }
  ,    //Thursday
  {
    10000,2000,12,19    }
  ,    //Friday
  {
    10000,10000,12,19    } 
}; //Saturday no peak period

void alloff()
{
  analogWrite(redPin, 255);
  analogWrite(greenPin, 255);
}

void colorLamp(int pwrlevel)
{

  int indred, indgreen;
  int thisHour, thisDay, redrange, greenrange, redpercent, greenpercent;
  long i, x, y, rangemax;
#define redbright 0            // set for level of brightness and range of color
#define greenbright 225
#define reddim 127
#define greendim 240
#define indicatorPin 5         // sample for ambient light in room

  thisHour = hour();
  thisDay = weekday()-1;
  if (thisHour >= pgm_read_dword_near(&pwrRangeArray[thisDay][PEAKSTART]) && 
    thisHour < pgm_read_dword_near(&pwrRangeArray[thisDay][PEAKEND]))
    rangemax = pgm_read_dword_near(&pwrRangeArray[thisDay][PEAKMAX]);
  else
    rangemax = pgm_read_dword_near(&pwrRangeArray[thisDay][OFFPEAKMAX]); // suck the value out of the table
  //  strcpy_P(Dbuf,PSTR("value from peak array is "));
  //  Serial.print(Dbuf);
  //  Serial.println(rangemax);
  x = (long)pwrlevel;                 //explicitly converting because I'm not sure
  x = x > rangemax ? rangemax : x;     //don't bother with over the max stuff

  y = 1024 - analogRead(indicatorPin); //check how bright the room is
  //  Serial.println(y); //debugging
  if (y >500){                        // to dim the led when the room lights are off
    redrange = reddim;
    greenrange = greendim;
//    Serial.print("Dim, ");
  }
  else {
    redrange = redbright;
    greenrange = greenbright;
//    Serial.print("Bright, ");
  }
  indred=map(x,rangemax,0,redrange,255);
  analogWrite(redPin,indred);
  indgreen=map(x,0,rangemax,greenrange,255);
  analogWrite(greenPin,indgreen);
//  Serial.print(x);
//  Serial.print(", ");
//  Serial.print(indred);
//  Serial.print(", ");
//  Serial.println(indgreen);
}

void setup() {

  Serial.begin(57600);          //talk to it port
  Serial.println("Init...");
  // color power indicator
  pinMode(redPin, OUTPUT);      // sets the pin as output for indicator LED
  pinMode(greenPin, OUTPUT);
  alloff();
  pinMode(powerLed, OUTPUT);    // indicator LEDs to tell me that data
  digitalWrite(powerLed, LOW);  // is coming in over XBee network
  pinMode(timeLed, OUTPUT);
  digitalWrite(powerLed, LOW);
  pinMode(statusLed, OUTPUT);
  digitalWrite(powerLed, LOW);
  
  pinMode(LCDtxPin, OUTPUT);     // The output pin for serial LCD
  lcdSerial.begin(9600);
  lcdSerial.print(0x7C, BYTE);   // Intensity to max
  lcdSerial.print(157, BYTE);
  delay(100);
  lcdSerial.print(0xFE, BYTE);   //command flag
  lcdSerial.print(0x01, BYTE);   //clear command.
  delay(100);
  strcpy_P(Dbuf, PSTR("Data Init..."));
  lcdSerial.print(Dbuf);    //let them know it's alive
  LCDline2();
  strcpy_P(Dbuf, PSTR("Time Init..."));
  lcdSerial.print(Dbuf);
  strcpy_P(Dbuf, PSTR("LCD Init..."));
  Serial.println(Dbuf);

//  wdt_enable(WDTO_8S);          // 
//  wdt_reset();                  // 
//  wdt_disable();

  xbeeSerial.begin(9600);
  strcpy_P(Dbuf,PSTR("Setup Complete"));
  Serial.println(Dbuf);
}

void initTimers(){
  Alarm.timerRepeat(60, showTime);   // update lcd clock every minute
  Alarm.timerRepeat(5, showPower);   // update power display every few seconds
  Alarm.timerRepeat(30, houseStatus); // this gets the outside and inside temp from controller
  Alarm.timerRepeat(15*60, showMem);  //keeping tabs on memory usage
}
  
boolean firsttime = true;
boolean alarmsSet = false;

void loop(){

  if(firsttime == true){  // keep from running out of memory!!
    showMem();
    firsttime = false;
    memset(xbeeIn,0,sizeof(xbeeIn));
  }
  if (timeStatus() != timeNotSet){
    tNow = now(); // just to be sure the time routines are ok
    if (!alarmsSet){
      initTimers();
      alarmsSet = true;
      Serial.println("Timers set up");
      showTime();
      showPower();
      houseStatus();
    }
    Alarm.delay(0); // and the alarm routines as well
  }
  xbeeSerial.listen();
  while (xbeeSerial.available()){
    checkXbee();
  }
}


