// To control the defrost cycle on my freezer.
//
// I have a freezer only, that means it doesn't have a refrigerator section
// and I monitor it's power with a IRIS smart plug.  Over time I noticed that 
// it was defrosting during the peak usage period of the day and causing a 
// load that was costing me too much money.

// this timer was created to read the time from my house clock and 
// cause the defrost circuitry to fire during low use periods.
// 
// This is the second version.
// Added a temperature sensor to the device and transmit the temp on every update
// Also coded to close and open BOTH relays. This way I can share the load and maybe, 
// get longer life out of the relays.
// 
// I'm up to version 3 now
// Paralleling the relays was a serious mistake. There's a blog post on the problems it
// led to. This version supports the new board I had made to hold a high current relay 
// module and a new defrost switch I added to that board. The switch will turn on add off
// the defroster so I can see what is happening i the future.
//
// Don't forget to change the version number (just after the defines)

#include <Time.h>
#include <TimeAlarms.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <Button.h>
#include <XBee.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define xbeeRxPin 2
#define xbeeTxPin 3
#define defrostRelay1Pin 4
#define defrostRelay2Pin 5

// The 18B20 is a onewire device *really three wires in this case pwr, ground, data)
// So twwo libraries support it OneWire and DallasTemperature
#define OneWireBus 8 // digital pin where one-wire reading is taken
/* Set up a oneWire instance to communicate with any OneWire device*/
OneWire oneWire(OneWireBus);
/* Tell Dallas Temperature Library to use oneWire Library */
DallasTemperature sensors(&oneWire);

//Temporary variable use in float conversion
char t[10], v[10];

//XBee parameters
char deviceName [21];  // read fromm the XBee that is attached
// XBee addresses this device is interested in
XBeeAddress64 Broadcast = XBeeAddress64(0x00000000, 0x0000ffff);
//XBeeAddress64 HouseController= XBeeAddress64(0x0013A200, 0x406f7f8c); // For reference
XBeeAddress64 Destination;  // This will hold the XBee address that the code currently sends to
XBeeAddress64 ThisDevice;   // will fill in with the local XBee address

char verNum[] ="Freezer Timer Version 3 Init...";
SoftwareSerial xbeeSerial = SoftwareSerial(xbeeRxPin, xbeeTxPin);

char Dbuf[50];
char Dbuf2[50];
char xbeeIn[100]; // accumulated XBee input
XBee xbee = XBee();
ZBRxResponse xbrx = ZBRxResponse();  // for holding the ascii traffic
ZBRxIoSampleResponse ioSample = ZBRxIoSampleResponse();  // for handling io samples

time_t tNow;

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

void(* resetFunc) (void) = 0; //declare reset function @ address 0

//
// The timers for the freezer are below
//
void initTimers(){
  Alarm.alarmRepeat(8,15,0, defrosterOn);  //turn on defroster (code will turn itself off)
  Alarm.alarmRepeat(20,15,0, defrosterOn); // run it twice a day in non-peak periods
  Alarm.timerRepeat(2*60,freezerReport);      //the "I'm alive message"
}
// These two timers will blink the light to tell me it's working
void aliveLedOn(){
  digitalWrite(13,HIGH); // Light on the board when ready to run
  Alarm.timerOnce(2, aliveLedOff);
}

void aliveLedOff(){
  digitalWrite(13,LOW); // Light on the board when ready to run
  Alarm.timerOnce(2, aliveLedOn);
}

AlarmID_t offTimer = dtNBR_ALARMS;  // just making sure it can't be allocated initially

void defrosterOn(){
  if (digitalRead(defrostRelay1Pin) == HIGH){
    freezerReport();
    return;  // it's already on, just return
  }
  digitalWrite(defrostRelay1Pin, HIGH); // turn on the defroster
  digitalWrite(defrostRelay2Pin, HIGH); // use both relays
  if(Alarm.isAllocated(offTimer)){ //if there is already a timer running, stop it.
    Alarm.free(offTimer);
    offTimer = dtNBR_ALARMS;       // to indicate there is no timer active
  }
  offTimer = Alarm.timerOnce(30*60, defrosterOff);  // automatically turn it off after timeout
  Serial.println("Defroster On");
  freezerReport();
}

void defrosterOff(){
  if (Alarm.isAllocated(offTimer)){ //If there is an off timer running, stop it.
    Alarm.free(offTimer);
    offTimer = dtNBR_ALARMS;
  }
  // doesn't hurt anything to turn it off multiple times.
  digitalWrite(defrostRelay1Pin, LOW); // turn the defroster off
  digitalWrite(defrostRelay2Pin, LOW); // using both relays
  Serial.println("Defroster Off");
  freezerReport();
}

boolean buttonPressed = false; // for interrupt handling the button
#define commandButton 9
Button button1 = Button(commandButton, PULLUP);

void setup() {

  Serial.begin(57600);          //talk to it port
  Serial.println(verNum);

  pinMode(defrostRelay1Pin, OUTPUT);
  pinMode(defrostRelay2Pin, OUTPUT);
  digitalWrite(defrostRelay1Pin, LOW);
  digitalWrite(defrostRelay2Pin, LOW);
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  analogReference(INTERNAL);   // hoping to improve the temp reading
  sensors.begin();
 
  xbeeSerial.begin(9600);
  xbee.setSerial(xbeeSerial);
  // Now get parameters from the XBee
  getDeviceParameters();
  Serial.print(F("This is device "));
  Serial.println(deviceName);
  Serial.print(F("This Device Address is 0x"));
  print32Bits(ThisDevice.getMsb());
  Serial.print(F(" 0x"));
  print32Bits(ThisDevice.getLsb());
  Serial.println();
  Serial.print(F("Default Destination Device Address is 0x"));
  print32Bits(Destination.getMsb());
  Serial.print(F(" 0x"));
  print32Bits(Destination.getLsb());
  Serial.println();

  
  strcpy_P(Dbuf,PSTR("Setup Complete"));
  Serial.println(Dbuf);
//  setTime(1296565408L);  // debuging
  wdt_enable(WDTO_8S);   // No more than 8 seconds of inactivity
}

boolean firsttime = true;
boolean alarmsSet = false;
char requestBuffer [100];
int requestIdx = 0;

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
      freezerReport(); // So I don't have to wait 2 minutes for it to report
      Serial.println("Timer set up");
    }
    Alarm.delay(0); // Just for the alarm routines
  }
  if (button1.uniquePress()){
    Serial.println(F("Button was pressed"));
    if (digitalRead(defrostRelay1Pin) == HIGH){ // The defroster is already on
      defrosterOff();
    }
    else {
      defrosterOn();
    }
  }

  if(Serial.available()){  // extremely primitive command decoder
    char c = Serial.read();
    switch(c){
      case '\r':  // carriage return, process string
        requestBuffer[requestIdx++] = c;
        requestBuffer[requestIdx] = '\0';
        Serial.println(requestBuffer);
        // now do something with the request string
        processXbee(requestBuffer, strlen(requestBuffer));
        //start collecting the next request string
        memset(requestBuffer,0,sizeof(requestBuffer));  //start off empty
        requestIdx = 0; // and at the beginning
        break;
      default:
        requestBuffer[requestIdx] = c;
        if(requestIdx < sizeof(requestBuffer))
          requestIdx++;
        else{  //the buffer filled up with garbage
          memset(requestBuffer,0,sizeof(requestBuffer));  //start off empty
          requestIdx = 0; // and at the beginning
        }
        break;
    }
  }

  // check for incoming commands or time update
  xbeeSerial.listen();
  while (xbeeSerial.available() > 0){
    checkXbee();
  }
  // made it, reset the watchdog timer
  wdt_reset();                   //watchdog timer set back to zero
}

