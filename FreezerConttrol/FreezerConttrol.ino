// To control the defrost cycle on my freezer.
//
// I have a freezer only, that means it doesn't have a refrigerator section
// and I monitor it's power with a IRIS smart plug.  Over time I noticed that 
// it was defrosting during the peak usage period of the day and causing a 
// load that was costing me too much money.

// this timer was created to read the time from my house clock and 
// cause the defrost circuitry to fire during low use periods.
// 
// This is the first version.
//
// Don't forget to change the version number (just after the defines)

#include <Time.h>
#include <TimeAlarms.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <Button.h>
#include <XBee.h>

#define xbeeRxPin 2
#define xbeeTxPin 3
#define defrostRelayPin 4

char verNum[] ="Freezer Timer Version 1 Init...";
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
  if (digitalRead(defrostRelayPin) == HIGH){
    freezerReport();
    return;  // it's already on, just return
  }
  digitalWrite(defrostRelayPin, HIGH); // turn on the defroster
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
  digitalWrite(defrostRelayPin, LOW); // turn the pump off
  Serial.println("Defroster Off");
  freezerReport();
}

void setup() {

  Serial.begin(57600);          //talk to it port
  Serial.println(verNum);

  pinMode(defrostRelayPin, OUTPUT);
  digitalWrite(defrostRelayPin, LOW);
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
 
  xbeeSerial.begin(9600);
  xbee.setSerial(xbeeSerial);
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

