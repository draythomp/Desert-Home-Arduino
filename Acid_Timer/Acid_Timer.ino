// My acid injection pump timer code.
// It gets the time from my XBee network and runs the injection pump
// based on alarms.  I also check to be sure the pool pump is on and 
// stop the injector if someone turns off the pool pump.
//
// Version 2: There is a button on the device to turn the injector on manually
// This will set a 10 minute timer to turn it back off.  This allows
// me to push the button and walk off.
// 
// Version 3: I put in the float to tell me when the acid level is low, and
// added code to tell me the numeric day of year so I can base timers on every
// other day, third day, fourth day, etc.  I also added the day of year to the status
// message after the level indicator so I can tell if it is supposed to dump.
//
// Version 4: Added an XBee command to turn the acid pump on for 10 minutes.  I'll 
// change the house controller to allow a button for this under the pool controls.
// Frankly, I got tired of going over and hunting for the paper clip to turn the
// pump on manually.
//
// Version 5: added blinking light to tell me it's working.  Also put in a watchdog
// and the fixes to the XBee code that fix the bug receiving collision packets from
// the XBee network.  
//
// After version 5 locked up and didn't come back I put in a reboot at midnight to see
// if there was something strange going on.  I also started printing out the version
// number in the signon so I could be sure I had put the latest code on the 
// device, So, this is version 6
//
// Version 7
// Still having trouble with it going down and not coming back. I realized that the 
// weather is quite a bit colder and it could be failing due to temperature.  However, 
// there may be some kind of bug in the watchdog that I haven't figured out yet.
// That brought up a problem with how to reprogram the ardweeny bootloader.  See, the 
// ardweeny uses the standard 328 bootloader.  This bootloader doesn't allow the 
// hardware bootloader to work properly so in version 6 I used the code I worked out 
// for the mega2560.  That code doesn't rely on the bootloader keeping the bits correct.
// However, it's my code and could have a bug that I haven't turned up in testing.  
// So, I'm switching to the hardware watchdog and that requires a bootloader that
// actually works.  Much experimentation ensued.  I figured out how to connect the 
// USBTinyISP to the Ardweeny and then put the Optiboot bootloader on the board.  It
// works fine, loads faster and the watchdog works.  I just have to remember to choose
// UNO from the board choices.
//
// However, this may not fix it.  If the problem is temperature related, I'll have to 
// fix some hardware to get it working, so I'm going to swap out the Ardweeny and use
// the new bootloader at the same time.  If it works for a few days, I'll put the old board
// back with the updated bootloader in it.  If it fails, I'll start looking at power supply
// and stuff like that.  Maybe get some freeze spray.
//
// The board swap above fixed it.  This code has a change in the XBee portion to use
// memcpy_P to move data from progmem, implement it in next version.
//
// Version 8: I added command to get the status of the acid pump.  Occasionally, the pump quits 
// reporting, but seems to be working fine.  This may be a problem with the communication 
// being jammed up with traffic.  I'll have to add code to the controller to request
// status when that happens to force an update.  Also added status update whenever a command
// is sent to the pump.  This way I can send an off command when it is already off and force
// an update if I'm testing something.
// 
// Version 9:  Turns out that multiple on commands will set multiple off timers that can then 
// time out and stop the pump.  So, experimenting with the pumps operation can cause it
// to shut off early.  This becomes a problem whenever I want to test it's operation.  By 
// turning it on, off, on I have two timers running to shut it off.  This version has 
// code to cancel the timer each time I shut it off to prevent that.
//
// Version 10:  When I modified the pool controller to send directly to the house controller
// I lost the pool motor status.  So, I modified the house controller to send the pool
// motor status as part of its status broadcast.  Eventually, I only want the time and
// house status to be the messages that are sent in broadcast with the house controller
// concentrating all the status of my devices.  Which means that this version also only sends
// the pump status to the house control and not broadcast. I also added the version ID define
// below because I couldn't remember to change the message inside
#define versionID 10

#include <Time.h>
#include <TimeAlarms.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <Button.h>

#define xbeeRxPin 2
#define xbeeTxPin 3
#define acidPump 4
#define startbutton 5
#define acidLevelPin 6

SoftwareSerial xbeeSerial = SoftwareSerial(xbeeRxPin, xbeeTxPin);
Button startButton = Button(startbutton,PULLUP);

char Dbuf[50];
char Dbuf2[50];
char xbeeIn[100]; // accumulated XBee input
time_t tNow;
boolean pumping = 0;
char *acidLevel = "Low";
struct PoolData{
  char motorState [10];
  char waterfallState [5];
  char lightState [5];
  char fountainState [5];
  char solarState [5];
  int poolTemp;
  int airTemp;
} poolData;

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

// Special code for a watchdog timer.
#define TIMEOUTPERIOD 10000             // You can make this time as long as you want,
                                       // it's not limited to 8 seconds like the normal
                                       // watchdog
#define doggieTickle() resetTime = millis();  // This macro will reset the timer.
unsigned long resetTime = 0;
void watchdogSetup()
{
cli();  // disable all interrupts
wdt_reset(); // reset the WDT timer
MCUSR &= ~(1<<WDRF);  // because the data sheet said to
/*
WDTCSR configuration:
WDIE = 1 Interrupt Enable
WDE = 1  Reset Enable - I won't be using this on the 2560
WDP3 = 0 For 1000ms Time-out
WDP2 = 1 bit pattern is 
WDP1 = 1 0110  change this for a different
WDP0 = 0 timeout period.
*/
// Enter Watchdog Configuration mode:
WDTCSR = (1<<WDCE) | (1<<WDE);
// Set Watchdog settings: interrupte enable, 0110 for timer
WDTCSR = (1<<WDIE) | (0<<WDP3) | (1<<WDP2) | (1<<WDP1) | (0<<WDP0);
sei();
}

void(* resetFunc) (void) = 0; //declare reset function @ address 0

ISR(WDT_vect) // Watchdog timer interrupt.
{ 
  if(millis() - resetTime > TIMEOUTPERIOD){
    resetFunc();
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

//
// The timers to control the pump are below
//
void initTimers(){
// It will start at 08:15 AM and the start routine will cause
// it to stop 10 minutes later.
// temporarily disable auto timer while pump is being repaired.
//  Alarm.alarmRepeat(8,15,0, autoDumpAcid);  //turn on Acid Pump (pump code will turn itself off)
  Alarm.timerRepeat(2*60,sendStatusXbee); //the "I'm alive message"
  Alarm.alarmRepeat(23,59,0, resetFunc);  // To stop the darn thing from hanging after several days.
}
// These two timers will blink the light to tell me it's working
void aliveLedOn(){
  digitalWrite(13,HIGH); // Light on the board when ready to run
  showMem();
  Alarm.timerOnce(2, aliveLedOff);
}

void aliveLedOff(){
  digitalWrite(13,LOW); // Light on the board when ready to run
  Alarm.timerOnce(2, aliveLedOn);
}


void autoDumpAcid(){
  if((dayOfYear() % 2) != 0)  // I only dump acid on even numbered days.
    return;
  if(strcmp(poolData.motorState, "Off") == 0){ // is the pool motor off?
    Alarm.timerOnce(60,autoDumpAcid);  // check back in a minute
    return;
  }
  acidPumpOn();
}
// this will set the timer number to one past the highest so I can check and see if
// the offTimer has been set to go off already.  This will stop me from setting multiple
// offTimers by checking to see if there is one already and cancelling it if there is
AlarmID_t offTimer = dtNBR_ALARMS;  // just making sure it can't be allocated initially

void acidPumpOn(){
  if (digitalRead(acidPump) == HIGH){
    sendStatusXbee();
    return;  // it's already on, just return
  }
  digitalWrite(acidPump, HIGH); // turn on the pump
  pumping = true;
  if(Alarm.isAllocated(offTimer)){ //if there is already a timer running, stop it.
    Alarm.free(offTimer);
    offTimer = dtNBR_ALARMS;       // to indicate there is no timer active
  }
  offTimer = Alarm.timerOnce(10*60, acidPumpOff);  // automatically turn it off in 10 min
  Serial.println("Acid Pump On");
  sendStatusXbee();
}

void acidPumpOff(){
  if (Alarm.isAllocated(offTimer)){ //If there is an off timer running, stop it.
    Alarm.free(offTimer);
    offTimer = dtNBR_ALARMS;
  }
  // doesn't hurt anything to turn it off multiple times.
  digitalWrite(acidPump, LOW); // turn the pump off
  pumping = false;
  Serial.println("Acid Pump Off");
  sendStatusXbee();
}

void setup() {

  Serial.begin(57600);          //talk to it port
  Serial.print("Combined Pool, Acid, Septic Controller Version:  ");
  Serial.println(versionID, DEC);
  Serial.println(" Init...");

  pinMode(acidPump, OUTPUT);
  digitalWrite(acidPump, LOW);
  pinMode(acidLevelPin, INPUT);
  digitalWrite(acidLevelPin, HIGH);
  pinMode(acidLevelPin, INPUT);
  if(digitalRead(acidLevelPin) != 0)
    strcpy(acidLevel, "Low");
  else
    strcpy(acidLevel, "OK");
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
 
  xbeeSerial.begin(9600);
//  doggieTickle();
//  watchdogSetup();
  strcpy_P(Dbuf,PSTR("Setup Complete"));
  Serial.println(Dbuf);
//  setTime(1296565408L);  // debuging
  wdt_enable(WDTO_8S);   // No more than 8 seconds of inactivity
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
    if (!alarmsSet){
      initTimers();
      alarmsSet = true;
      aliveLedOn(); // Light on the board when ready to run (blinks)
      sendStatusXbee(); // So I don't have to wait 2 minutes for it to report
      Serial.println("Timers set up");
    }
    Alarm.delay(0); // Just for the alarm routines
  }
  if (startButton.uniquePress()){  //this prevents bounce
    if ((digitalRead(acidPump) == LOW) && (strcmp(poolData.motorState, "Off") != 0)){
      acidPumpOn();
    }
    else {
      acidPumpOff();
    }
  }
  // Check the float to see what the level is
  if(digitalRead(acidLevelPin) == 0)
    strcpy(acidLevel, "LOW");
  else
    strcpy(acidLevel, "OK");

  xbeeSerial.listen();
  while (xbeeSerial.available()){
    checkXbee();
  }
  // made it, reset the watchdog timer
//  doggieTickle();
  wdt_reset();                   //watchdog timer set back to zero
}

