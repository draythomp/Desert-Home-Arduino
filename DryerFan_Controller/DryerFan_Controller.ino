// My Dryer Fan Controller code.
// This will sense the current in the close dryer feed and turn on the fan.
// It will light the LED and also sense the attic temperature and report it.


#include <Time.h>
#include <TimeAlarms.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <XBee.h>

char verNum[] = "Version 1.1";

#define xbeeRxPin 2
#define xbeeTxPin 3

SoftwareSerial xbeeSerial = SoftwareSerial(xbeeRxPin, xbeeTxPin);
XBee xbee = XBee();
ZBRxResponse xbrx = ZBRxResponse();  // for holding the ascii traffic


char Dbuf[50];
char Dbuf2[50];
char xbeeIn[100]; // accumulated XBee input
float temperatureC;
float temperatureF;

void(* resetFunc) (void) = 0; //declare reset function @ address 0
//
// Data variables that I will use to monitor the garage
//
#define garageDoor1 8  // the two garage door sensors
#define garageDoor2 9
#define garageDoorButton1 4  //This is like pushing the button on the door opener
#define garageDoorButton2 5
#define waterHeater 6
//
//Attic Temperature sensor is Analog pin 1
//
int sensorPin=1;
//
// The timers to control various operations are below
//
void initTimers(){
  Alarm.timerRepeat(5,sendStatusXbee);    // Garage report every three seconds
  Alarm.alarmRepeat(23,59,0, resetFunc);  // To stop the darn thing from hanging after several days.
  Alarm.alarmRepeat(11,59,0, waterHeaterOff);
  Alarm.alarmRepeat(19,1,0, waterHeaterOn);
}

void setup() {

  Serial.begin(57600);          //talk to it port
  Serial.print("Dryer Fan Controller ");
  Serial.print(verNum);
  Serial.println(" Init...");

  setTime(11,30,0,11,5,2015);

  pinMode(13, OUTPUT);  // turn off the on board 'I'm alive' led
  digitalWrite(13, LOW);
  // enable internal pull up resistor and set input pins for garage doors
  pinMode(garageDoor1, INPUT);           // set pin to input
  digitalWrite(garageDoor1, HIGH);       // turn on pullup resistors
  pinMode(garageDoor2, INPUT);           // Other door also
  digitalWrite(garageDoor2, HIGH);       // 
  // the garage doors sensors will be shorted to ground when the door is
  // closed and show a high when the door is open
  
  // Relays 1 and 2 will control the door opening and closing
  // there is only a toggle for this; if it's open, a relay can close it
  // and if it's closed the relay will open it
  pinMode(garageDoorButton1, OUTPUT);
  digitalWrite(garageDoorButton1,HIGH);
  pinMode(garageDoorButton2, OUTPUT);
  digitalWrite(garageDoorButton2,HIGH);
  pinMode(waterHeater, OUTPUT);
  digitalWrite(waterHeater, HIGH);
  // garage door initialization complete
  
  xbeeSerial.begin(9600);  // This starts the xbee up
  xbee.setSerial(xbeeSerial);  // XBee library will use Software Serial port
  strcpy_P(Dbuf,PSTR("Setup Complete"));
  Serial.println(Dbuf);
  wdt_enable(WDTO_8S);   // No more than 8 seconds of inactivity
}

boolean firsttime = true;
boolean alarmsSet = false;

void loop(){

  if(firsttime == true){  // keep from running out of memory!!
    showMem();
    firsttime = false;
    //this just starts the XBee buffer off clean
    memset(xbeeIn,0,sizeof(xbeeIn));
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
  // made it, reset the watchdog timer
  wdt_reset();                   //watchdog timer set back to zero
}
