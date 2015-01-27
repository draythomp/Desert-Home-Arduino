/*
 Old note: I need to allow for Pachube to fail and still control stuff
 inside the house.  This will entail removing the failsafe from
 pachube.  I think I still need the failsafes for the internal
 network though.  Update, this item is done.  Also allowed the response to 
 be received in the main loop so I can check later if it becomes
 necessary.  
 
 With Version 19 I have a command to turn the acid pump on for 10 minutes.
 and added a timer to run the pump on odd numbered days.  The acid pump itself
 has a timer to run on even days.  I wanted a minimum injection as a default.
 
 Remember that pins 4 and 10 are taken by the ethernet card to control the SD
 card and the ethernet chip (respectively).  To choose which one, the pins are 
 active low.
 
 Major updates to ethernet handling and XBee packet recieve code.  Many untraceable
 problems fixed after I discovered that using print caused many tiny packets that would 
 lock up the ethernet board for long periods.  When I had it stable on the ethernet, the
 other stuff related to XBee handling showed up and I was able to fix it.
 
 Unfortunately, I have now added several routines to the ethernet library to support
 things like timing out the connection request and being able to get the incoming
 mac and ip address back to the application level.  These changes will have to be 
 ported forward to any new versions of the IDE.
 
 Version 21 is where I started things like Winter Night.  A mode that sets the overnight 
 temperature.  More research is needed on the 'best' temperature for nights.  Also,
 is there an evening? and day?  I suspect that evening will be the next addition and I'll
 probably be basing the inside temperature on the outside temp.  For example, 70 feels cold 
 when the temp outside is 100.  I probably don't need either the heater or cooler for the
 range of 68-85 during the day.  These are things I'll have to test.
 
 Version 22 is where the watchdog timer was updated.  I hated having to use Timer3 for this
 especially when I eventually want other hardware timers enabled.  It also could change the 
 way PWM and SoftwareSerial work on certain pins.  I got a clue from the Arduino forum on
 using the watchdog timer as a timed interrupt and did some research and testing finally
 coming up with a nice solution that allows watchdogs that can run for a very long time if
 needed.  It's documented on my blog as well.
 
 Version 23 only has the baud rate change for the XBee serial port to 57600.  Actually, because
 of a bug in the Arduino software, I set the baud rate to 58824 baud to get it to work.
 
 Version 24 Just making changes to the timers for summer. 
 
 Version 25 Added presets for summer night, winter night, and A/C off.  This makes it easy to
 set it for bedtime and turn the A/C off when running the swamp cooler or leaving the doors open.
 
 Version 26 Found out the charting on Pachube (now COSM) has improved a lot.  Modified the
 HTTP screen to handle this.  Should be a more pleasant display.  Also am adding buttons for 
 turning off the peak handling so I can use the AC on holidays.
 
 Version 27 added feature to check and see if the pool is reporting in (like the Acid Pump)
 
 Version 28 added garage door reporting and ability to open and close garage door using my 
 brand new shiny Garage Controller.  I also added centering to the LCD display so I didn't have
 to be so careful with formatting.
 
 Version 28 this is where I finally worked out a way to turn items on and off on the pool
 without having to use the stupid toggle crap.  I can just set it to high speed and the pool 
 interface will take the steps necessary.  Not tested completely yet, but it works so far. 
 However, it changed a bunch of commands so this controller has to change to match.
 
 Version 29 I added a status command that is whenever the acid pump fails to report.  This
 should wake up the conversation more rapidly whenever the pump stops being seen.  This took
 changes in both the pump software and the controller.
 
 Version 30, added the pool motor state to the status message.  This is needed
 because the pool doesn't broadcast anymore and the acid pump needs it.
 
 Version 31, this is where I became disappointed enough to experiment with emoncms as a cloud
 service for data storage instead of Cosm (pachube).  Cosm has been getting slower and slower
 to the point where I can't retrieve even a days worth of data without the server failing.  Also,
 they haven't lived up to promises regarding charts and data capabilities.  So, time to experiment
 with something else.
 
 emoncms has become a thriving open source energy monitoring site, and is worth experimenting with.
 So, currently I'm going to parallel the updates to the cloud with Cosm and try the service out.

 Version 32, kept having trouble with the pool controller locking up.  Added the reset command to 
 controller so I could do some experimenting.  It will show up as a new button on the pool.
 
 Version 35:  Adding data upload to thinkspeak.  I just discovered this site and want to try it out.
 I now support thingspeak, cosm, and emoncms.
 
 Version 36: Need an alert light on the device to tell me that something, somewhere is not right.
 
 Version 37: First pass at controlling the hot water heater.  The garage controller will report it 
 in the garage line and the ability to see if power is applied is added to the house controller.  
 The garage is supposed to shut it off during peak periods.  The house controller can turn it on at
 anytime, but a power failure will override the house because the garage will come up with the power
 off.
 
 Version 2.1:  This is the first revision based on the Arduino IDE changing.  There have been major
 changes to the ethernet handling.  The arduino team combined DNS and DHCP with the ethernet library
 which made some things much easier, but very different from the previous version.  I also found a 
 way to limit the time the ethernet chip waited for a connection; this made it really easy to limit 
 waiting for a connection for all parts of the code.  This should eliminate the problems I had with 
 almost a minute timeout.  Almost everything is tied to strings, so I have to be careful not to get
 into problems with memory fragmentation.  There were several changes to the code for the lcd 
 display; they changed the Serial.print worked so I had to go to Serial.write for the control 
 commands.  Also had to enable interrupts after the watchdog timeout to get the debug message to
 complete; the serial changes require interrupts to work.  I did make changes to the ethernet library
 to support obtaining the remote ip and mac, they still haven't added that capability to the library.
  
 This version functions the same as the previous version, just under a new compiler and library set
 
 Version 2.3, debugging the pool controller going nuts.  I also added the No Report change to make it
 red.  This makes it show up better when checking for problems.  I extended the web page to show the 
 last update time for the various data storage web sites I've been testing.
 
 Be sure to update the version number each time (just below the defines)
*/
#include <SPI.h>
#include <Ethernet.h>
#include <utility/w5100.h>
//#include <EthernetDNS.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <MemoryFree.h>
#include <SoftwareSerial.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <EEPROM.h>
#include "Ztemplate.h"  //I had to add this because the compiler changed (it's in a tab)
#include <TextFinder.h>
#include <XBee.h>

char verNum[] = "Version 2.4";

#define ethernetResetPin 2  // used to reset the ethernet board
#define rxSense 3           // used to check the state of the ethernet board
#define lcdDisplayTx 49      // serial lcd display
#define lcdDisplayRx 48      // I don't use this, but it has to be sent to NSS

SoftwareSerial lcdDisplay (lcdDisplayRx, lcdDisplayTx);

//Status LEDs so I can see what's happening
// first the communication LEDs
#define timeRxLed 25
#define tempRxLed 26
#define powerRxLed 24
#define etherRxLed 27
#define lookForProblemLed 28
#define poolRxLed 29
#define statusRxLed 30
#define securityOffLed 31
#define extEtherRxLed 39
// Now the heat pump status LEDs
#define nRecircLed 36
#define nCoolLed 34
#define nHeatLed 32
#define sRecircLed 37
#define sCoolLed 35
#define sHeatLed 33

struct savedstuff   //data that needs to be saved across reboots
{
  long magic_number;
  int  highestTemp;
  int  lowestTemp;
  byte lastIp[4];
} eepromData, tempEepromData;

unsigned long resetTime = 0;
#define TIMEOUTPERIOD 15000             // You can make this time as long as you want,
                                       // it's not limited to 8 seconds like the normal
                                       // watchdog
#define doggieTickle() resetTime = millis();  // This macro will reset the timer.

time_t pachubeUpdateTime = 0;  // track the time the servers were last updated.
time_t emoncmsUpdateTime = 0;
time_t thingspeakUpdateTime = 0;

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {0x90, 0xA2, 0xDA, 0x00, 0x33, 0x33};
//byte gateway_ip[] = {192,168,0,1};	// router or gateway IP address
byte ip[] = {192,168,0,205};
byte subnet_mask[] = {255,255,255,0};	// subnet mask for the local network
byte externalIp[] = {0,0,0,0};  //the device's external ip address

// This is used to establish dns service for device's external access
EthernetClient ipCheck;
EthernetClient dyndns;
TextFinder ipfinder(ipCheck,999);
// the pachube connection
EthernetClient pachube;
TextFinder pachubeFinder(pachube,99);
// emoncms connection
EthernetClient emoncms;
TextFinder emoncmsFinder(emoncms,99);
// thingspeak connection
EthernetClient thingspeak;
TextFinder thingspeakFinder(thingspeak);

char Dbuf[100];    //general purpose buffer for strings
char Dbuf2[100];  // this is only for sprintf stuff
char etherIn[50]; // accumulated ethernet input
char xbeeIn[100]; // accumulated XBee input

float realPower,apparentPower,powerFactor,rmsCurrent,rmsVoltage,frequency;
struct GarageData{
  time_t reportTime;
  char door1 [10];
  char door2 [10];
  char waterHeater [5];
} garageData;

struct PoolData{
  time_t reportTime;
  char motorState [10];
  char waterfallState [5];
  char lightState [5];
  char fountainState [5];
  char solarState [5];
  int poolTemp;
  int airTemp;
} poolData;

struct AcidPumpData{
  time_t reportTime;
  char state [5];
  char level [10];
} acidPumpData;

time_t tNow;

// Initialize the Ethernet server library
// with the IP address and port 
EthernetServer Control(80);

// Thermostats  0 is the North one and 1 is the South
IPAddress NThermoAddress(192,168,0,202);
IPAddress SThermoAddress(192,168,0,203);
struct thermoData{
  time_t reportTime;
  char name[10];
  int currentTemp;
  char state[10];
  int tempSetting;
  char mode[10];
  char fan[10];
  char peak[10];
} ThermoData[2];
struct OutSideSensor{
  time_t reportTime;
  float temp;
} outsideSensor;

// XBee stuff
XBee xbee = XBee();
ZBRxResponse xbrx = ZBRxResponse();  // for holding the ascii traffic
ZBRxIoSampleResponse ioSample = ZBRxIoSampleResponse();  // for handling io samples

void showMem(){
  strcpy_P(Dbuf,PSTR("Mem = "));
  Serial.print(Dbuf);
  Serial.println(freeMemory());
}

void eepromAdjust(){
  EEPROM_readAnything(0, tempEepromData); // get saved settings
  if((eepromData.magic_number != tempEepromData.magic_number) ||
    (eepromData.highestTemp != tempEepromData.highestTemp) ||
    (eepromData.lowestTemp != tempEepromData.lowestTemp) ||
    (memcmp(eepromData.lastIp, tempEepromData.lastIp,4) != 0)){
      strcpy_P(Dbuf, PSTR("eeprom Data updated"));
      Serial.println(Dbuf);
      EEPROM_writeAnything(0, eepromData);
  }
}

// Special code for a 2560 watchdog timer.
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

ISR(WDT_vect) // Watchdog timer interrupt.
{ 
  if(millis() - resetTime > TIMEOUTPERIOD){
    sei();  // turn on interrupts to allow serial output.
    resetMe("watchdog timer");
  }
}

void setup()
{
  doggieTickle(); // oddly enough, I have to do this
  watchdogSetup();  // my failsafe code
  int errorCount = 0;
  Serial.begin(57600);
  strcpy_P(Dbuf,PSTR("********************Initializing..********************"));
  Serial.println(Dbuf);
  EEPROM_readAnything(0,eepromData);
  if (eepromData.magic_number != 1234){ //initialize the eeprom first time
    eepromData.magic_number = 1234;
    eepromData.highestTemp = 0;
    eepromData.lowestTemp = 0;
    memcpy (eepromData.lastIp, externalIp,4);
    EEPROM_writeAnything(0,eepromData);
    strcpy_P(Dbuf,PSTR("Initialized EEPROM "));
    Serial.print(Dbuf);
  }
  lcdDisplay.begin(9600);
  lcdInit();
  lcdLine1Print(verNum);
  // set up the Status LEDs
  initLeds();
  // start the Ethernet connection and the server:
  Serial.println(F("Ethernet.."));
  doggieTickle();  // these things can take some time
  lcdLine2Print("Ethernet..");
  pinMode(10,OUTPUT);   // this chooses the ethernet card for SPI interaction
  digitalWrite(10,LOW); // although the ethernet code does this as well.
  pinMode(4,OUTPUT);    // this turns off the SD card so it doesn't kill the ethernet
  digitalWrite(4,HIGH);
  ethernetReset();
  doggieTickle();
  Ethernet.begin(mac, ip);  // statically assign ip address so I know where it is
  W5100.setRetransmissionTime(2000);  // sets up a short timeout for ethernet connections
  W5100.setRetransmissionCount(3); 
  Serial.print(F("Working at IP address "));
  Serial.println(Ethernet.localIP());
  doggieTickle();
  // get previously published external ip address
  checkPublicIp();
  // Start the ethernet server
  Control.begin();   
  Serial.print(F("XBee.."));
  Serial1.begin(58824);  // this is actually 57600 baud (arduino bug)
  xbee.setSerial(Serial1);  // XBee library will use Serial port 1
  Serial.println(F("Done"));
  Serial.println("All Done.");
  lcdLine2Print("Complete");
  doggieTickle();
}

int etherIndex = 0;
boolean firsttime = true;
boolean alarmsSet = false;
int maxAlarms = 0;

void loop(){
  if(firsttime == true){
    Serial.print(F("I'm alive "));
    showMem();
    firsttime = false;
    memset(xbeeIn,0,sizeof(xbeeIn));
    doggieTickle();
  }

  if( Alarm.count() > maxAlarms){  // keep track of the number of alarms used
    maxAlarms = Alarm.count();
//    Serial.print("Alarms at: ");
//    Serial.println(maxAlarms);
  }
  doggieTickle(); // This tickles the watchdog

  // if you don't wait until the time is set the timers will fire
  // this could be bad if you have an alarm that you don't expect to go 
  // off when you first start up.
  if (timeStatus() != timeNotSet){
    tNow = now(); // just to be sure the time routines are ok
    if (!alarmsSet){
      initTimers();
      alarmsSet = true;
      Serial.println(F("Timers set up"));
      lcdClear();
      lcdLine1Print("Timers Set up");
    }
    Alarm.delay(0); // and the alarm routines as well
  }

  // First, check for XBee updates to the power and time
  while (Serial1.available() > 0){ //check for XBee input
    checkXbee();
  }

  if (pachube.available()){  //for debugging pachube interaction
//    Serial.print("Pachube Response: ");
    if (pachube.available()){
      if(pachubeFinder.find("HTTP/1.1")){
        int status = (byte)pachubeFinder.getValue();
        Serial.print(F("Pachube returned "));
        Serial.println(status);
      }
      else {
        Serial.println(F("Odd response from Pachube"));
      }
    }
    pachube.flush(); //don't currently care about the rest of it
    pachube.stop();
  }

  if (emoncms.available()){  //for debugging thingspeak interaction
//    Serial.print("emoncms Response: ");
    if (emoncms.available()){
      if(emoncmsFinder.find("ok")){
        Serial.println(F("emoncms returned OK"));
      }
      else {
        Serial.println(F("Odd response from emoncms"));
      }
    }
    emoncms.flush(); //don't currently care about the rest of it
    emoncms.stop();
  }
  if (thingspeak.available()){  //for debugging thingspeak interaction
//    Serial.print("thingspeak Response: ");
    if (thingspeak.available()){
      if(thingspeakFinder.find("HTTP/1.1")){
        int status = (byte)thingspeakFinder.getValue();
        Serial.print(F("thingspeak returned "));
        Serial.println(status);
      }
      else {
        Serial.println(F("Odd response from thingspeak"));
      }
    }
    thingspeak.flush(); //don't currently care about the rest of it
    thingspeak.stop();
  }

  // listen for incoming clients that want to know what is happening.
  if (EthernetClient ControlClient = Control.available()) {  // is there a client connected?
    memset(etherIn,0,sizeof(etherIn));
    etherIndex=0;
    while (ControlClient.connected()){  // get whatever it was they sent
      if (ControlClient.available()) {
        char c = ControlClient.read();
        // if you've gotten to the end of the line (received a newline
        // character) you can send a reply
        if (c != '\n' && c!= '\r') {
          etherIn[etherIndex++] = c;
          if (etherIndex >= sizeof(etherIn)) // max size of buffer
            etherIndex = sizeof(etherIn) - 1;
          continue;
        }
        etherIn[etherIndex] = 0;  // null terminate the line for parsing
        ControlClient.flush();    // I really don't care about the rest
        if(isLocal(ControlClient)){
          strcpy_P(Dbuf,PSTR("Local "));
        }
        else {
          strcpy_P(Dbuf, PSTR("Outside "));
        }
        strcat_P(Dbuf, PSTR("Machine Request:"));
        Serial.println(Dbuf);
        digitalWrite(etherRxLed,LOW);          // indicator for internet activity
        Alarm.timerOnce(1,etherRxLedOff);      // this will turn it off in a second
        if(!isLocal(ControlClient)){
          digitalWrite(extEtherRxLed,LOW);     // indicator for external access
          Alarm.timerOnce(1,extEtherRxLedOff); // and turn it off in a second too
        }
        Serial.println(etherIn);
        processEtherIn(ControlClient, etherIn, etherIndex); // go parse the stuff they sent
        break;
      }
     }
      // give the web browser time to receive the data
      Alarm.delay(1);                           // actually this isn't needed, but it checks alarms
      // close the connection:
      ControlClient.stop();                     // signal client that we're done
    }
}
// turn the security back on after timer fires
// I allow someone outside to change things for a time period
// and then automatically shut it off so I don't forget
boolean securityOn = true;

void turnSecurityOn(){
  securityOn = true;
  Serial.println(F("Security back on"));
}

void processEtherIn(EthernetClient ControlClient, char *etherIn, int etherIndex){
  char* buf;
  
  // standard http response header
  strcpy_P(Dbuf, PSTR("HTTP/1.1 200 OK\n"));
  ControlClient.write(Dbuf);
  strcpy_P(Dbuf, PSTR("Content-Type: text/html\n\n"));
  ControlClient.write(Dbuf);
  if (strstr_P(etherIn, PSTR("GET / ")) != 0){ // root level request, send status
    sendStatus(ControlClient);
    return;
  }
  else if (strstr_P(etherIn,PSTR("GET /show")) != 0){ // alternative display command
    sendStatus(ControlClient);
    return;
  }
  else if (strstr_P(etherIn,PSTR("GET /favicon.ico")) != 0){  // stupid double hit for this thing
    return;
  }
  else if (strstr_P(etherIn, PSTR("GET /LETMEIN")) != 0){    // super secret password for external access
    if(securityOn == true){  // you don't get to extend the time.
      Alarm.timerOnce(5*60,turnSecurityOn);
      digitalWrite(securityOffLed,LOW);
      strcpy_P(Dbuf,PSTR("Security off"));
      Serial.println(Dbuf);
      securityOn = false;
    }
    sendStatus(ControlClient);
    return;
  }
  //
  // exclude external machines from making changes
  // unless the machine has the secret command
  //
  if (!isLocal(ControlClient) && securityOn){ // not a local machine, bye
    strcpy_P(Dbuf,PSTR("Denied\n"));
    Serial.println(Dbuf);
    Serial.println(etherIn); // interesting to know what they tried.
    ControlClient.write(Dbuf);
    return;
  }
  else if (strstr_P(etherIn,PSTR("GET /reset")) != 0){ // remote reset, just for the heck of it
    resetMe("remote reset");
    return;  //Won't ever get here 
  }
  else if ((buf=strstr_P(etherIn,PSTR("GET /"))) != 0){
    int i = 0;
    char *data, *thing, *state, *where;
    char command[20];
    
    data = strtok_r(buf, "/", &buf);   // skip to actual command
    thing = strtok_r(0, ",", &buf);    // which thing is being changed
    state = strtok_r(0, ",", &buf);    // how it's being changed
    where = strtok_r(0, ",", &buf);  // where it is
    //
    // Process the garage door commands
    //
    if (strncmp_P(thing,PSTR("door1close"),10) == 0){
      closeGarageDoor1();
      redirectBack(ControlClient);
      return;
    }
    if (strncmp_P(thing,PSTR("door2close"),10) == 0){
      closeGarageDoor2();
      redirectBack(ControlClient);
      return;
    }
    if (strncmp_P(thing,PSTR("door1open"),9) == 0){
      openGarageDoor1();
      redirectBack(ControlClient);
      return;
    }
    if (strncmp_P(thing,PSTR("door2open"),9) == 0){
      openGarageDoor2();
      redirectBack(ControlClient);
      return;
    }
    if (strncmp_P(thing,PSTR("waterheateron"),13) == 0){
      waterHeaterOn();
      redirectBack(ControlClient);
      return;
    }
    if (strncmp_P(thing,PSTR("waterheateroff"),14) == 0){
      waterHeaterOff();
      redirectBack(ControlClient);
      return;
    }
    //
    // Now the special A/C control buttons
    //
    if (strncmp_P(thing,PSTR("acoff"),5) == 0){
      airoff();
      getThermostat(1, "status", true);
      getThermostat(0, "status", true);
      redirectBack(ControlClient);
      return;
    }
    if (strncmp_P(thing,PSTR("summernight"),11) == 0){
      summerNight();
      getThermostat(1, "status", true);
      getThermostat(0, "status", true);
      redirectBack(ControlClient);
      return;
    }
    if (strncmp_P(thing,PSTR("winternight"),11) == 0){
      winterNight();
      getThermostat(1, "status", true);
      getThermostat(0, "status", true);
      redirectBack(ControlClient);
      return;
    }
    if (strncmp_P(thing,PSTR("temp=98"),7) == 0){
      heatpumpTo98();
      getThermostat(1, "status", true);
      getThermostat(0, "status", true);
      redirectBack(ControlClient);
      return;
    }
    if (strncmp_P(thing,PSTR("peakOn"),6) == 0){ // turn peak handling on
      peakOn();
      getThermostat(1, "status", true);
      getThermostat(0, "status", true);
      redirectBack(ControlClient);
      return;
    }
    if (strncmp_P(thing,PSTR("peakOff"),7) == 0){  // turn peak handling off
      peakOff();
      getThermostat(1, "status", true);
      getThermostat(0, "status", true);
      redirectBack(ControlClient);
      return;
    }
    if (strncmp_P(thing,PSTR("temp"),4) == 0){
      if(strncmp_P(state,PSTR("up"),2) == 0){
        strcpy_P(command, PSTR("+"));
      }
      else if (strncmp_P(state,PSTR("down"),4) == 0){
        strcpy(command, "-");
      }
      else {
        commandError(ControlClient);
        return;
      }
      if(strncmp_P(where,PSTR("n"),1) == 0){
        getThermostat(0, command, false);
      }
      else if(strncmp_P(where, PSTR("s"),1) == 0){
        getThermostat(1, command, false);
      }
      else{
        commandError(ControlClient);
        return;
      }
      getThermostat(1, "status", true);
      getThermostat(0, "status", true);
      redirectBack(ControlClient);
      return;
    }
    else if (strncmp_P(thing,PSTR("fan"),3) == 0){
      if(strncmp_P(state,PSTR("on"),2) == 0){
        strcpy_P(command, PSTR("fan=on"));
      }
      else if (strncmp_P(state,PSTR("auto"),4) == 0){
        strcpy_P(command, PSTR("fan=auto"));
      }
      else if (strncmp_P(state,PSTR("recirc"),6) == 0){
        strcpy_P(command, PSTR("fan=recirc"));
      }
      else {
        commandError(ControlClient);
        return;
      }
      if(strncmp_P(where,PSTR("n"),1) == 0){
        getThermostat(0, command, false);
      }
      else if(strncmp_P(where, PSTR("s"),1) == 0){
        getThermostat(1, command, false);
      }
      else{
        commandError(ControlClient);
        return;
      }
      getThermostat(1, "status", true);
      getThermostat(0, "status", true);
      redirectBack(ControlClient);
      return;
    }
    else if (strncmp_P(thing,PSTR("mode"),4) == 0){
      if(strncmp_P(state,PSTR("heat"),4) == 0){
        strcpy_P(command, PSTR("heat"));
      }
      else if (strncmp_P(state,PSTR("cool"),4) == 0){
        strcpy_P(command, PSTR("cool"));
      }
      else if (strncmp_P(state,PSTR("off"),3) == 0){
        strcpy_P(command, PSTR("off"));
      }
      else {
        commandError(ControlClient);
        return;
      }
      if(strncmp_P(where,PSTR("n"),1) == 0){
        getThermostat(0, command, false);
      }
      else if(strncmp_P(where, PSTR("s"),1) == 0){
        getThermostat(1, command, false);
      }
      else{
        commandError(ControlClient);
        return;
      }
      getThermostat(1, "status", true);
      getThermostat(0, "status", true);
      redirectBack(ControlClient);
      return;
    }
    else if (strncmp_P(thing,PSTR("pool"),4) == 0){
//      Serial.println("Pool command");
      if(strncmp_P(state,PSTR("motorOff"),8) == 0){
//        Serial.println("motor off");
        poolMotorOff();
        redirectBack(ControlClient);
      }
      else if(strncmp_P(state,PSTR("motorHigh"),9) == 0){
//        Serial.println("motor high");
        poolMotorHigh();
        redirectBack(ControlClient);
      }
      else if(strncmp_P(state,PSTR("motorLow"),8) == 0){
//        Serial.println("motor low");
        poolMotorLow();
        redirectBack(ControlClient);
      }
      else if(strncmp_P(state,PSTR("waterfallOn"),11) == 0){
//        Serial.println("waterfall on");
        poolWaterfallOn();
        redirectBack(ControlClient);
      }
      else if(strncmp_P(state,PSTR("waterfallOff"),12) == 0){
//        Serial.println("waterfall off");
        poolWaterfallOff();
        redirectBack(ControlClient);
      }
      else if(strncmp_P(state,PSTR("fountainOn"),10) == 0){
//        Serial.println("fountain on");
        poolFountainOn();
        redirectBack(ControlClient);
      }
      else if(strncmp_P(state,PSTR("fountainOff"),11) == 0){
//        Serial.println("fountain off");
        poolFountainOff();
        redirectBack(ControlClient);
      }
      else if(strncmp_P(state,PSTR("lightOn"),7) == 0){
//        Serial.println("light on");
        poolLightOn();
        redirectBack(ControlClient);
      }
      else if(strncmp_P(state,PSTR("lightOff"),8) == 0){
//        Serial.println("light off");
        poolLightOff();
        redirectBack(ControlClient);
      }
      else if(strncmp_P(state,PSTR("controllerReset"),15) == 0){
//        Serial.println("Controller Reset");
        poolControllerReset();
        redirectBack(ControlClient);
      }

      else if(strncmp_P(state,PSTR("acidOn"),6) == 0){
//        Serial.println("Acid on");
        acidPumpOn();
        redirectBack(ControlClient);
      }
      else if(strncmp_P(state,PSTR("acidOff"),7) == 0){
//        Serial.println("Acid off");
        acidPumpOff();
        redirectBack(ControlClient);
       }
      else {
        commandError(ControlClient);
      }
      return;
    }
    else {
      commandError(ControlClient);
      return;
    }
  }
  else{
    commandError(ControlClient);
    return;
  }
}

void commandError(EthernetClient client){
  strcpy_P(Dbuf,PSTR("command error\n"));
  client.write(Dbuf);
}

void redirectBack(EthernetClient ControlClient){
  
  strcpy_P(Dbuf, PSTR("<strong>Hold on a few seconds while I do that...</strong>"));
  ControlClient.write(Dbuf);
  strcpy_P(Dbuf, PSTR("<meta http-equiv=\"refresh\" content=\"5; URL=show\">"));
  ControlClient.write(Dbuf);
}

void sendStatus(EthernetClient ControlClient){
  
    // standard http response header
    strcpy_P(Dbuf, PSTR("<html><title>Desert Home</title>"));
    ControlClient.write(Dbuf);
    // put in a favicon that is held on an external site
    strcpy_P(Dbuf, PSTR("<LINK REL=\"SHORTCUT ICON\" HREF=\"http://dl.dropbox.com/u/128855213/favicon.ico\" />"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf, PSTR("<head><style type=\"text/css\"></style></head>"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf, PSTR("<body><h1><center>Desert Home Controller<br>"));
    ControlClient.write(Dbuf);
    // output the time
    tNow = now();
    strcpy_P(Dbuf2,PSTR("%02d/%02d/%4d<br></center></h1><h2>"));
    sprintf(Dbuf,Dbuf2,month(tNow),day(tNow),year(tNow));
    ControlClient.write(Dbuf);
    //
    // Here is where I insert the SteelSeries gauges
    //
    strcpy_P(Dbuf,PSTR("<div>"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf,PSTR("<iframe height=\"550\" width=\"100%\" scrolling=\"No\" seamless frameBorder=\"0\" "));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf,PSTR("src=\"http://dl.dropbox.com/u/128855213/LocalDetailPage.htm\"></iframe></div>"));
    ControlClient.write(Dbuf);
    //
    // End of the gauge insert
    //
    // First the garage door report
    //
    strcpy_P(Dbuf, PSTR("<div>"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf, PSTR("Garage Doors:"));
    if(abs(garageData.reportTime - now()) >= (5*60)){  // has it been 15 minutes since last report?
      strcat_P(Dbuf,PSTR("<span style=\"color:red;\";>No Report</span><br>"));
      ControlClient.write(Dbuf);
    }
    else {
      strcat_P(Dbuf,PSTR("<br>"));
      ControlClient.write(Dbuf);
      if (strcmp_P(garageData.door1, PSTR("open")) != 0){
        strcpy_P(Dbuf, PSTR("<span style=\"color:black;\">Door 1 is Closed</span>&nbsp;&nbsp;"));
        ControlClient.write(Dbuf);
        strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='door1open'\">Open</button>"));
      }
      else {
        strcpy_P(Dbuf, PSTR("<span style=\"color:red;\">Door 1 is Open</span>&nbsp;&nbsp;"));
        ControlClient.write(Dbuf);
        strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='door1close'\">Close</button>"));
      }
      ControlClient.write(Dbuf);
      // Done reporting door number 1
      strcpy_P(Dbuf, PSTR("&nbsp;&nbsp;"));
      ControlClient.write(Dbuf);
      // now report door number 2
      if (strcmp_P(garageData.door2, PSTR("open")) != 0){
        strcpy_P(Dbuf, PSTR("<span style=\"color:black;\">Door 2 is Closed</span>&nbsp;&nbsp;"));
        ControlClient.write(Dbuf);
        strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='door2open'\">Open</button>"));
      }
      else {
        strcpy_P(Dbuf, PSTR("<span style=\"color:red;\">Door 2 is Open</span>&nbsp;&nbsp;"));
        ControlClient.write(Dbuf);
        strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='door2close'\">Close</button>"));
      }
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("<br>"));
      ControlClient.write(Dbuf);
      if (strcmp_P(garageData.waterHeater, PSTR("on")) != 0){
        strcpy_P(Dbuf, PSTR("<span style=\"color:black;\">Water heater is off</span>&nbsp;&nbsp;"));
        ControlClient.write(Dbuf);
        strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='waterheateron'\">On</button>"));
      }
      else {
        strcpy_P(Dbuf, PSTR("<span style=\"color:red;\">Water heater has power</span>&nbsp;&nbsp;"));
        ControlClient.write(Dbuf);
        strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='waterheateroff'\">Off</button>"));
      }
      strcat_P(Dbuf, PSTR("<br>"));
      ControlClient.write(Dbuf);
    }
    //
    // AC report
    //
    strcpy_P(Dbuf, PSTR("<div>"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='acoff'\">A/C System Off</button>"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='temp=98'\">Temp=98</button>"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='summernight'\">Summer Night</button>"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='winternight'\">Winter Night</button>"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='peakOff'\">No Peak Handling</button>"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='peakOn'\">Peak Handling</button></br>"));
    ControlClient.write(Dbuf);

    for ( int i=0; i<=1; i++){
      strcpy_P(Dbuf,PSTR("<p class=\"ex\">"));
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2,PSTR("<img src=\"%s\" alt=\"FAN\" align=\"right\" align=\"middle\" width=\"50\" height=\"50\" />"));
      if(strcmp_P(ThermoData[i].state, PSTR("Recirc")) == 0){
        sprintf(Dbuf,Dbuf2,"http://tinyurl.com/3umagvq");
      }
      else if(strcmp_P(ThermoData[i].state, PSTR("Heating")) == 0){
        sprintf(Dbuf,Dbuf2,"http://tinyurl.com/63zc8gb");
      }
      else if(strcmp_P(ThermoData[i].state, PSTR("Cooling")) == 0){
        sprintf(Dbuf,Dbuf2,"http://tinyurl.com/3n95mqz");
      }
      else if (strcmp_P(ThermoData[i].state, PSTR("Idle")) == 0){
        sprintf(Dbuf,Dbuf2,"http://tinyurl.com/44h9o4c");
      }
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2,PSTR("%s Thermostat:<br>"));
      sprintf(Dbuf,Dbuf2, i==0 ? "North" : "South");
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2,PSTR("Temp is %d, Heat pump is %s, <br>Temp Setting is %d, <br>Mode %s, <br>Fan %s, <br>%s Period<br>"));
      sprintf(Dbuf,Dbuf2, ThermoData[i].currentTemp,
        ThermoData[i].state,
        ThermoData[i].tempSetting,
        ThermoData[i].mode,
        ThermoData[i].fan,
        ThermoData[i].peak);
//        for(int i = 0; i < strlen(Dbuf); i++){ //I once used this to test the ethernet board
//          ControlClient.print(Dbuf[i]);
//        }
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2, PSTR("<button onClick=\"window.location='temp,up,%s'\"><img src=\"http://tinyurl.com/4yqtv5a\"></button>"));
      sprintf(Dbuf,Dbuf2, i==0 ? "n" : "s");
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2, PSTR("<button onClick=\"window.location='temp,down,%s'\"><img src=\"http://tinyurl.com/3vr29ek\"></button>"));
      sprintf(Dbuf,Dbuf2, i==0 ? "n" : "s");
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2, PSTR("<button onClick=\"window.location='fan,auto,%s'\"><img src=\"http://tinyurl.com/3v49j26\"></button>"));
      sprintf(Dbuf,Dbuf2, i==0 ? "n" : "s");
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2, PSTR("<button onClick=\"window.location='fan,on,%s'\"><img src=\"http://tinyurl.com/3wfcrmr\"></button>"));
      sprintf(Dbuf,Dbuf2, i==0 ? "n" : "s");
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2, PSTR("<button onClick=\"window.location='fan,recirc,%s'\"><img src=\"http://tinyurl.com/3u3qewu\"></button>"));
      sprintf(Dbuf,Dbuf2, i==0 ? "n" : "s");
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2, PSTR("<button onClick=\"window.location='mode,off,%s'\"><img src=\"http://tinyurl.com/3vdjdbg\"></button>"));
      sprintf(Dbuf,Dbuf2, i==0 ? "n" : "s");
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2, PSTR("<button onClick=\"window.location='mode,heat,%s'\"><img src=\"http://tinyurl.com/3swle62\"></button>"));
      sprintf(Dbuf,Dbuf2, i==0 ? "n" : "s");
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2, PSTR("<button onClick=\"window.location='mode,cool,%s'\"><img src=\"http://tinyurl.com/44fyvmt\"></button>"));
      sprintf(Dbuf,Dbuf2, i==0 ? "n" : "s");
      ControlClient.write(Dbuf);
      // these are the URLs for the animated fans
      // black stationary http://tinyurl.com/44h9o4c
      // red fan http://tinyurl.com/63zc8gb
      // green fan http://tinyurl.com/3umagvq
      // blue fan http://tinyurl.com/3n95mqz
      strcpy_P(Dbuf, PSTR("<br><p>"));
      ControlClient.write(Dbuf);
    }
    // tell me about the Acid Pump
    strcpy_P(Dbuf, PSTR("</div>"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf2, PSTR("Acid Pump: "));
    if(abs(acidPumpData.reportTime - now()) >= (5*60)){  // has it been 15 minutes since last report?
      strcat_P(Dbuf2,PSTR("<span style=\"color:red;\";>No Report</span><br>"));
      ControlClient.write(Dbuf2);
      acidPumpStatus(); // no report in a while, wake up the conversation
    }
    else {
      strcat_P(Dbuf2, PSTR("is %s, level is "));
      sprintf(Dbuf, Dbuf2, acidPumpData.state);
      ControlClient.write(Dbuf);
      // if it's Low, print in Red
      if(strcmp_P(acidPumpData.level, PSTR("LOW")) == 0){
        strcpy_P(Dbuf, PSTR("<span style=\"color:red;\";>LOW</span>"));
      }
      else {
        strcpy_P(Dbuf, PSTR("<span style=\"color:black;\";>OK</span>"));
      }
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("&nbsp;&nbsp;"));
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='pool,acidOn'\">Acid On</button>"));
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='pool,acidOff'\">Acid Off</button>"));
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("<br>"));
      ControlClient.write(Dbuf);
    }
    // pool Status 
    strcpy_P(Dbuf, PSTR("<div>"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf, PSTR("<br>Pool: "));
    ControlClient.write(Dbuf);
    // check to see if there's been a recent report
    if(abs(poolData.reportTime - now()) >= (5*60)){  // has it been 15 minutes since last report?
      strcpy_P(Dbuf,PSTR("<span style=\"color:red;\";>No Report</span><br>"));
      ControlClient.write(Dbuf);
    }
    else {
      strcpy_P(Dbuf2, PSTR("<br>Motor is %s <br>"));
      sprintf(Dbuf, Dbuf2, poolData.motorState);
      ControlClient.write(Dbuf);
      // if the motor isn't running, the temperature is invalid
      if (strcmp_P(poolData.motorState, PSTR("Off")) != 0){
        strcpy_P(Dbuf2, PSTR("Water Temperature is %d <br>"));
        sprintf(Dbuf, Dbuf2, poolData.poolTemp);
        ControlClient.write(Dbuf);
      }
      strcpy_P(Dbuf2, PSTR("Light is %s <br>"));
      sprintf(Dbuf, Dbuf2, poolData.lightState);
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2, PSTR("Solar Heating is %s <br>"));
      sprintf(Dbuf, Dbuf2, poolData.solarState);
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2, PSTR("Waterfall is %s <br>"));
      sprintf(Dbuf, Dbuf2, poolData.waterfallState);
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf2, PSTR("Fountain is %s <br>"));
      sprintf(Dbuf, Dbuf2, poolData.fountainState);
      ControlClient.write(Dbuf);
      // now, put the control buttons up
      strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='pool,motorOff'\">Motor Off</button>"));
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='pool,motorHigh'\">Motor High</button>"));
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='pool,motorLow'\">Motor Low</button>"));
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='pool,waterfallOn '\">Waterfall On</button>"));
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='pool,waterfallOff'\">Waterfall Off</button>"));
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='pool,fountainOn'\">Fountain On</button>"));
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='pool,fountainOff'\">Fountain Off</button>"));
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='pool,lightOn'\">Light On</button>"));
      ControlClient.write(Dbuf);
      strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='pool,lightOff'\">Light Off</button>"));
      ControlClient.write(Dbuf);
    }
    strcpy_P(Dbuf, PSTR("<button onClick=\"window.location='pool,controllerReset'\">Control Reset</button>"));
    ControlClient.write(Dbuf);
    // now report on the cloud data repositories
    strcpy_P(Dbuf, PSTR("<br><br><div>Data Storage Status:<br>"));
    ControlClient.write(Dbuf);
    // First report on Cosm
    strcpy_P(Dbuf, PSTR("Cosm Update Time: "));
    ControlClient.write(Dbuf);
    if(abs(pachubeUpdateTime - now()) >= (15*60)){  // has it been 15 minutes since last report?
      strcpy_P(Dbuf,PSTR("<span style=\"color:red;\";>No Report</span>"));
      ControlClient.write(Dbuf);
    }
    else{
      strcpy_P(Dbuf2,PSTR("%2d:%02d"));
      sprintf(Dbuf, Dbuf2, hour(pachubeUpdateTime), minute(pachubeUpdateTime));
      ControlClient.write(Dbuf);
    }
    strcpy_P(Dbuf, PSTR("<br>"));
    ControlClient.write(Dbuf);
    // Next emoncms report
    strcpy_P(Dbuf, PSTR("Emoncms Update Time: "));
    ControlClient.write(Dbuf);
    if(abs(emoncmsUpdateTime - now()) >= (15*60)){  // has it been 15 minutes since last report?
      strcpy_P(Dbuf,PSTR("<span style=\"color:red;\";>No Report</span>"));
      ControlClient.write(Dbuf);
    }
    else{
      strcpy_P(Dbuf2,PSTR("%2d:%02d"));
      sprintf(Dbuf, Dbuf2, hour(emoncmsUpdateTime), minute(emoncmsUpdateTime));
      ControlClient.write(Dbuf);
    }
    strcpy_P(Dbuf, PSTR("<br>"));
    ControlClient.write(Dbuf);
    // Next thingspeak report
    strcpy_P(Dbuf, PSTR("ThingSpeak Update Time: "));
    ControlClient.write(Dbuf);
    if(abs(thingspeakUpdateTime - now()) >= (15*60)){  // has it been 15 minutes since last report?
      strcpy_P(Dbuf,PSTR("<span style=\"color:red;\";>No Report</span>"));
      ControlClient.write(Dbuf);
    }
    else{
      strcpy_P(Dbuf2,PSTR("%2d:%02d"));
      sprintf(Dbuf, Dbuf2, hour(thingspeakUpdateTime), minute(thingspeakUpdateTime));
      ControlClient.write(Dbuf);
    }
    strcpy_P(Dbuf, PSTR("<br>"));
    ControlClient.write(Dbuf);
    strcpy_P(Dbuf, PSTR("</div>"));
    ControlClient.write(Dbuf); // end of the data storage report division
    // all done with the web page
    strcpy_P(Dbuf, PSTR("<br></body></h2></head></html>\n"));
    ControlClient.write(Dbuf);
}
