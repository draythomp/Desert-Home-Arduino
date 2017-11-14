// Temperature Monitor
// Measures the temperature and sends it over the XBee network.
// Puts the Arduino and XBee to sleep after each send to conserve
// battery power. On vs off time is set with the defines below.
//
// Program as Pro mini 5V 16MHz ATmega 328
// For documentation see desert-home.com 
//
// Don't forget to change the version number (below includes)
//#include <Time.h>
//#include <TimeAlarms.h>
#include <SoftwareSerial.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <XBee.h>
#include <JeeLib.h> // Low power functions library is here
#include <OneWire.h>

#include <DallasTemperature.h>
#include <Button.h>
#include <ArduinoJson.h>
 
char verNum[] = "Version 1";
char t[10], v[10], pt[10];  // To store the strings for temp and pressure

// The awake and sleep times
#define AWAKETIME 5000 
#define SLEEPTIME 115000
//#define SLEEPTIME 15
unsigned long savedmillis = 0; // Use this to time the sleep cycles
unsigned long actuallyAwake = 0; // because I extend the time with the button

// The tmp36 pin
#define OneWireBus 8 // analog pin where reading is taken
#define voltagePin A0

// These are the XBee control pins
#define xbeeRxPin 4
#define xbeeTxPin 5
#define xbeeCTS 6         // Clear to send
#define xbeeSleepReq 7    // Set low to wake it up
#define AWAKE LOW         // I can never rememmber it, so
#define SLEEP HIGH        // I made these defines 

/* Set up a oneWire instance to communicate with any OneWire device*/
OneWire oneWire(OneWireBus);
/* Tell Dallas Temperature Library to use oneWire Library */
DallasTemperature sensors(&oneWire);

// The various XBee messages and such
SoftwareSerial xbeeSerial = SoftwareSerial(xbeeRxPin, xbeeTxPin);
XBee xbee = XBee();
ZBRxResponse xbrx = ZBRxResponse();  // for holding the ascii traffic
AtCommandRequest atRequest = AtCommandRequest();
AtCommandResponse atResponse = AtCommandResponse();
ZBTxStatusResponse txStatus = ZBTxStatusResponse(); // to look at the tx response packet
ModemStatusResponse modemStatus = ModemStatusResponse();
char deviceType [] = "TempSensor";
char deviceName [21];  // read fromm the XBee that is attached
int deviceFirmware;
XBeeAddress64 Broadcast = XBeeAddress64(0x00000000, 0x0000ffff);
XBeeAddress64 RaspberryPi = XBeeAddress64(0x0013a200, 0x406f7f8C);// House Controller address, so I can remember it
XBeeAddress64 Destination;  // This will hold the XBee address that the code currently sends to
XBeeAddress64 ThisDevice;   // will fill in with the local XBee address
XBeeAddress64 Controller;   // will fill in with the default destination address read from the XBee

char requestBuffer [50]; // for requests coming from the serial port
int requestIdx;

char Dbuf[100]; // general purpose buffer

// Need this for the watchdog timer wake up operation
ISR(WDT_vect) { Sleepy::watchdogEvent(); } // Setup the watchdog

void(* resetFunc) (void) = 0; //declare reset function @ address 0

boolean buttonPressed = false; // for interrupt handling the button
#define commandButton 2
Button button1 = Button(commandButton, PULLUP);

void setup() {
  Serial.begin(9600);          //talk to it port
  Serial.print(F("Room Temperature Sensor "));
  Serial.print(verNum);
  Serial.println(F(" Init..."));

  pinMode(13, OUTPUT);         // turn off the on board led
  digitalWrite(13, LOW);       // because it uses power too
  analogReference(INTERNAL);   // hoping to improve the temp reading
  pinMode(xbeeCTS, INPUT);
  pinMode(xbeeSleepReq, OUTPUT);
  digitalWrite(xbeeSleepReq, AWAKE); // To make sure the XBee is awake
  xbeeSerial.begin(9600);            // This starts the xbee up
  xbee.setSerial(xbeeSerial);        // XBee library will use Software Serial port
  Destination = Broadcast;           // set XBee destination address to Broadcast for now
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
  Destination = Controller; // Setup to send to house controller
  
  sensors.begin();
  
  pinMode(2, INPUT);
  digitalWrite (2, HIGH);  // enable pull-up

  
  savedmillis = millis();
  Serial.println(F("Setup Complete"));
}

boolean firsttime = true;

void loop(){
  if(firsttime == true){  // keep from running out of memory!!
    showMem();
    firsttime = false;
  }
  /* Since this may have piled up a bunch of messages while
     sleeping, it could take a while
  */
  // check for incoming XBee traffic
  while (xbeeSerial.available() > 0)
    checkXbee();
    
  if (millis() - savedmillis > AWAKETIME){
    Serial.print("was awake for ");
    Serial.println(millis() - actuallyAwake);
    delay(100); // delay to allow the characters to get out
    savedmillis = millis();
    digitalWrite(xbeeSleepReq, SLEEP); // put the XBee to sleep
    while (digitalRead(xbeeCTS) != SLEEP){} // Wait 'til it actually goes to sleep
    unsigned long timeSlept = 0;
    int result = 1;
    while (timeSlept < SLEEPTIME){
      attachInterrupt(0, buttonThing, LOW);
      result = Sleepy::loseSomeTime((unsigned int)7000);
      if (result == 0) // this is something other than a watchdog
        break;
      timeSlept = (millis() - savedmillis);
    }
    Serial.print("was asleep for ");
    Serial.println(millis() - savedmillis);
    savedmillis = millis();
    actuallyAwake = millis();
    if (result == 0)
      Serial.println("Woke up on a button press");
    digitalWrite(xbeeSleepReq, AWAKE); // wake that boy up now
    sendStatusXbee();
  }
  // Because I am impatient, I put this in to catch another button press
  // before timing out and going to sleep. When I catch a second button press,
  // I extend the time for another 5 seconds in case I do it again.
  if (button1.uniquePress()){
    Serial.println(F("Button was pressed again"));
    savedmillis = millis(); // give it another 5 seconds
    buttonPressed = 1;
    sendStatusXbee();
  }
}
