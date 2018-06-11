// Water heater monitor and control based on the PZEM-004T code
//
// This takes on the device name from the XBee that is attached, and
// uses a PZEM004 to read the power usage from whatever it is monitoring.
// It also uses two 18B20 temperature sensors mounted on the top and bottom
// of the water heater
// It formats a JSON message like"
//
// {"DeviceName":{"V":119.10,"I":0.320,"P":41.000,"E":280.000,"TT":130,"BT":110}}
//
// It works by a timer firing to read the power data, and another timer
// controls the reporting time. The XBee is monitored constantly to get whatever
// is going on at the time. The two timers can be adjusted as needed in the
// code.
// The software serial library can't monitor both software ports, so I had
// to do it this way. Switching between the two sources empties the input
// buffer and stops everything from working.
//
// The one wire temperature sensors are handled the same way, by setting a timer to
// make the measurements, but they are done on minute basis because they don't change
// very fast.
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
#include <ArduinoJson.h>
#include <PZEM004T.h>

#define xbeeRxPin 2
#define xbeeTxPin 3
#define ONE_WIRE_BUS 4
#define waterHeater 6
#define pzemRxPin 10
#define pzemTxPin 11

#define reportInterval 30
#define readInterval 15
#define tempInterval 60

//XBee parameters
char deviceName [21];  // read fromm the XBee that is attached
// XBee addresses this device is interested in
XBeeAddress64 Broadcast = XBeeAddress64(0x00000000, 0x0000ffff);
//XBeeAddress64 HouseController= XBeeAddress64(0x0013A200, 0x406f7f8c); // For reference
XBeeAddress64 Destination;  // This will hold the XBee address that the code currently sends to
XBeeAddress64 ThisDevice;   // will fill in with the local XBee address

char verNum[] ="Water Heater Monitor and Control, version 1";
SoftwareSerial xbeeSerial = SoftwareSerial(xbeeRxPin, xbeeTxPin);
SoftwareSerial pMon(10,11);

char Dbuf[75];
//char Dbuf2[50];
char xbeeIn[100]; // accumulated XBee input
XBee xbee = XBee();
ZBRxResponse xbrx = ZBRxResponse();  // for holding the ascii traffic
ZBRxIoSampleResponse ioSample = ZBRxIoSampleResponse();  // for handling io samples

PZEM004T pzem(pzemRxPin,pzemTxPin);  // (RX,TX) connect to TX,RX of PZEM
IPAddress ip(192,168,1,1);

DeviceAddress topThermometer = { 0x28,0xFF,0xB3,0xD3,0x67,0x14,0x03,0x2A };
DeviceAddress bottomThermometer   = { 0x28,0xFF,0x4C,0x9A,0x67,0x14,0x03,0x90 };
// for the temperature measurements
#define TEMPERATURE_PRECISION 9
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
// arrays to hold device addresses for the top and bottom sensors

time_t tNow;

//
// The timers
// One to read the temperature sensors, one to read the power usage
// and one to report the data for logging and looking
//
void initTimers(){
  Alarm.timerRepeat(reportInterval,Report); //the power status message
  Alarm.timerRepeat(readInterval,getPower);
  Alarm.timerRepeat(tempInterval,getTemps);
  Alarm.alarmRepeat(11,59,0, waterHeaterOff); // failsafe for shutting the water heater off during peak
  Alarm.alarmRepeat(20,1,0, waterHeaterOn);   // usage periods. 
}
// These two timers will blink the light to tell me it's working
// turn on the light and set a timer to turn it off, then in the off
// routine, set a timer to turn it back on. This is simply a more 
// sophisticated 'blink' than the example. It allows for blinking
// lights without tieing up the processor
void aliveLedOn(){
  digitalWrite(13,HIGH); // Light on the board when ready to run
  Alarm.timerOnce(2, aliveLedOff);
}

void aliveLedOff(){
  digitalWrite(13,LOW); // Light on the board when ready to run
  Alarm.timerOnce(2, aliveLedOn);
}

// These global variables are the ones reported back for logging
float Current = 0.0;    // The instantaneous current reading in amps
float Voltage = 0.0;    // The instantaneous voltage reading in volts
float Power = 0.0;      // Measured in kW instaneous power usage
float Energy = 0.0;     // Measured in kWh the actual consumed energy
float topTemp = 0.0;    // Measured in degrees fahrenheit, the top thermometer
float bottomTemp = 0.0; // Measured in degrees fahrenheit, the bottom thermometer


// Reads the pzem-004 by requesting each measured item one at a time
// The power monitor may have other ways of reading the data, but I haven't 
// been able to get it. There are some interesting artifacts involved. 
// For example, you can read two or three of them quickly, but then it will
// pause in responding for a full second or more before the next response.
// This may be because the device is tied up doing a measurement or calculation
// during this period. It does make for a few problems reading the device.
// So, I read it as a separate effort based on a timer and hold the readings
// in global variables for later logging.
void getPower(){
  Serial.print(F("Power Readings: "));
  Voltage = pzem.voltage(ip);
  Serial.print(Voltage);Serial.print(F("V; "));
  
  Current = pzem.current(ip);
  Serial.print(Current);Serial.print(F("A; "));
 
  Power = pzem.power(ip);
  Serial.print(Power);Serial.print(F("W; ")); 
  
  Energy = pzem.energy(ip);
  Serial.print(Energy);Serial.print(F("Wh; "));
  Serial.println();
}

// Just a basic read of the two temperature sensors on the heater.
void getTemps(){
  Serial.print(F("Requesting temperatures..."));
  sensors.requestTemperatures();
  Serial.println(F("DONE"));
  Serial.println(F("Getting Top Temperature, "));
  topTemp = getTemp(topThermometer);
  Serial.println(F("Getting Bottom Temperature, "));
  bottomTemp = getTemp(bottomThermometer);
}

void setup() {

  Serial.begin(9600);          // Hardware Serial Port
  Serial.println(verNum);

  Serial.println(F("Initializing XBee"));
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  
  pinMode(waterHeater, INPUT);           // Turn on pullup
  digitalWrite(waterHeater, HIGH);       // 
  pinMode(waterHeater, OUTPUT);
  digitalWrite(waterHeater, LOW);
  
  xbeeSerial.begin(9600);      // Software XBee Port
  xbee.setSerial(xbeeSerial);
  initXBee();
  
  Serial.println(F("Initializing Thermometers"));
  // Start up the one wire library
  sensors.begin();

  // locate devices on the bus
  Serial.print(F("Locating devices..."));
  Serial.print(F("Found "));
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(F(" devices."));

  // report parasite power requirements
  Serial.print(F("Parasite power is: ")); 
  if (sensors.isParasitePowerMode()) 
    Serial.println(F("ON"));
  else 
    Serial.println(F("OFF"));

  Serial.print(F("Top Temp Sensor (0) Address: "));
  printAddress(topThermometer);
  Serial.println();

  Serial.print(F("Bottom Temp Sensor (1) Address: "));
  printAddress(bottomThermometer);
  Serial.println();

  // set the resolution to 9 bit
  sensors.setResolution(topThermometer, TEMPERATURE_PRECISION);
  sensors.setResolution(bottomThermometer, TEMPERATURE_PRECISION);
  Serial.print(F("Top Sensor Resolution: "));
  Serial.print(sensors.getResolution(topThermometer), DEC); 
  Serial.println();

  Serial.print(F("Bottom Sensor Resolution: "));
  Serial.print(sensors.getResolution(bottomThermometer), DEC); 
  Serial.println();
  
// Initial reads so I have good data for first send
  Serial.println(F("Reading initial data"));
  getTemps();
  getPower();
  showMem();
  //while(1){}
//  
  Serial.println(F("Setup Complete"));
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
      Serial.println(F("Timer set up"));
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

