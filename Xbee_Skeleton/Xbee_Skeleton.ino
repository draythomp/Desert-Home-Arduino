// To monitor the temperature of an appliance
//
//
// Don't forget to change the version number (just after the defines)

#include <Time.h>
#include <TimeAlarms.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <XBee.h>
#define xbeeRxPin 2
#define xbeeTxPin 3

//XBee parameters
char deviceName [21];  // read fromm the XBee that is attached
// XBee addresses this device is interested in
XBeeAddress64 Broadcast = XBeeAddress64(0x00000000, 0x0000ffff);
//XBeeAddress64 HouseController= XBeeAddress64(0x0013A200, 0x406f7f8c); // For reference
XBeeAddress64 Destination;  // This will hold the XBee address that the code currently sends to
XBeeAddress64 ThisDevice;   // will fill in with the local XBee address

char verNum[] ="XBee skeleton Init...";
SoftwareSerial xbeeSerial = SoftwareSerial(xbeeRxPin, xbeeTxPin);

char Dbuf[50];
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
// The timers
//
void initTimers(){
  Alarm.timerRepeat(2*60,Report);      //the message send timer
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

void setup() {

  Serial.begin(57600);          //hardware serial port
  Serial.println(verNum);

  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
 
  xbeeSerial.begin(9600);
  xbee.setSerial(xbeeSerial);
  initXBee();
  
  strcpy_P(Dbuf,PSTR("Setup Complete"));
  Serial.println(Dbuf);
//  setTime(1296565408L);  // debuging when the time isn't received
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
      Report(); // So I don't have to wait minutes for it to report
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

