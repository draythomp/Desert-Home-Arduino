// My Barometer code.
// It gets the time from my XBee network, and uses it to time the 
// XBee sends, midnight resets, and to stamp the data sent.
//
// Don't forget to change the version number (below includes)
#include <Time.h>
#include <TimeAlarms.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <XBee.h>
#include "Wire.h"
#include "Adafruit_BMP085.h"
 
Adafruit_BMP085 bmp;
char t[10], p[10];  // To store the strings for temp and pressure
float altitude = 711.1;
char verNum[] = "Version 1";

#define xbeeRxPin 2
#define xbeeTxPin 3

SoftwareSerial xbeeSerial = SoftwareSerial(xbeeRxPin, xbeeTxPin);
XBee xbee = XBee();
ZBRxResponse xbrx = ZBRxResponse();  // for holding the ascii traffic
AtCommandRequest atRequest = AtCommandRequest();
AtCommandResponse atResponse = AtCommandResponse();
ZBTxStatusResponse txStatus = ZBTxStatusResponse(); // to look at the tx response packet
char deviceName [21];  // read fromm the XBee that is attached
int deviceFirmware;
XBeeAddress64 Broadcast = XBeeAddress64(0x00000000, 0x0000ffff);
XBeeAddress64 Destination;  // This will hold the XBee address that the code currently sends to
XBeeAddress64 ThisDevice;   // will fill in with the local XBee address
XBeeAddress64 Controller;   // will fill in with the default destination address read from the XBee

char requestBuffer [50]; // for requests coming from the serial port
int requestIdx;
time_t tNow;  // just a place to keep the time
char Dbuf[100]; // general purpose buffer

void(* resetFunc) (void) = 0; //declare reset function @ address 0
//
// The timers to control operations are below
//
void initTimers(){
  Alarm.timerRepeat(60,sendStatusXbee);    // Garage report every three seconds
  Alarm.alarmRepeat(23,59,0, resetFunc);  // To stop the darn thing from hanging after several days.
}

void setup() {
  Serial.begin(115200);          //talk to it port
  Serial.print(F("Barometer "));
  Serial.print(verNum);
  Serial.println(F(" Init..."));

  pinMode(13, OUTPUT);         // turn off the on board 'I'm alive' led
  digitalWrite(13, LOW);
  xbeeSerial.begin(9600);  // This starts the xbee up
  xbee.setSerial(xbeeSerial);  // XBee library will use Software Serial port
  Destination = Broadcast;     // set XBee destination address to Broadcast for now
  // Now, get the unit name from the XBee, this will be sent in 
  // all reports.  While we're there, get some other stuff as well.
  xbeeSerial.listen();  // so SoftwareSerial pays attention to the XBee
  getDeviceParameters();
  Serial.print(F("This is device "));
  Serial.println(deviceName);
  Serial.print(F("Device Firmware version "));
  Serial.println(deviceFirmware,HEX);
  Serial.print(F("This Device Address is 0x"));
  print32Bits(ThisDevice.getMsb());
  Serial.print(F(" 0x"));
  print32Bits(ThisDevice.getLsb());
  Serial.println();
  Serial.print(F("Default Destination Device Address is 0x"));
  print32Bits(Controller.getMsb());
  Serial.print(F(" 0x"));
  print32Bits(Controller.getLsb());
  Serial.println();
  Destination = Controller;     // set XBee destination

  bmp.begin();  // Initialize the barometer
  Serial.println(F("Setup Complete"));
  wdt_enable(WDTO_8S);   // No more than 8 seconds of inactivity
}

boolean firsttime = true;
boolean alarmsSet = false;

void loop(){
    

  if(firsttime == true){  // keep from running out of memory!!
    showMem();
    float temperature = bmp.readTemperature()* 9/5 +32;
    float pressure = bmp.readPressure()/100.0;
    sprintf(Dbuf, "%s: temperature:%s, uncorrected pressure:%s\n", deviceName,
            dtostrf(temperature, 5, 1, t),dtostrf(pressure, 6, 1, p));
    Serial.print(Dbuf);
    firsttime = false;
  }
  // wait for house clock to give me the time
  if (timeStatus() != timeNotSet){
    // time is set so alarms can be initialized
    if (!alarmsSet){
      initTimers();
      alarmsSet = true;
      aliveLedOn(); // Light on the board when ready to run (blinks)
      sendStatusXbee(); // So I don't have to wait 2 minutes for it to report
      Serial.println("Timers set up");
    }
    Alarm.delay(0); // Just for the alarm routines
  }
  // check for incoming XBee traffic
  while (xbeeSerial.available() > 0)
    checkXbee();
  
    // This allows emulation of an incoming XBee command from the serial console
  // It's not a command interpreter, it's limited to the same command string as
  // the XBee.  Can be used for testing
  if(Serial.available()){  // 
    char c = Serial.read();
    if(requestIdx < sizeof(requestBuffer)){
      requestBuffer[requestIdx] = c;
      requestIdx++;
    }
    else{  //the buffer filled up with garbage
      memset(requestBuffer,0,sizeof(requestBuffer));  //start off empty
      requestIdx = 0; // and at the beginning
      c = 0; // clear away the just-read character
    }
    if (c == '\r'){
      requestBuffer[requestIdx] = '\0';
//      Serial.println(requestBuffer);
      // now do something with the request string
      processXbee(requestBuffer,strlen(requestBuffer));
      //start collecting the next request string
      memset(requestBuffer,0,sizeof(requestBuffer));  //start off empty
      requestIdx = 0; // and at the beginning
    }
  }

  // made it, reset the watchdog timer
  wdt_reset();                   //watchdog timer set back to zero
}
