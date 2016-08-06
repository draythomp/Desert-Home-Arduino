// To monitor the temperature of an appliance
//
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

char verNum[] ="Appliance Temp Monitor Version 1 Init...";
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
  Alarm.timerRepeat(2*60,Report);      //the "I'm alive message"
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

  Serial.begin(57600);          //talk to it port
  Serial.println(verNum);

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
      Report(); // So I don't have to wait 2 minutes for it to report
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

