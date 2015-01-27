/*

Goldline ProLogic control software.

This took a couple of months research and testing to get this far.  The pool controller
protocol is not documented anywhere, but there are a few subtle clues in various places.  I
put a LOT of comments in here to show what I'm doing and how I'm controlling the pool.

It's very difficult to decode the protocol and I haven't completed doing so.  However, I have
discovered enough to control my pool and you should be able to expand on my efforts to get 
what you need done.

I want to be able to set up timers to turn the pump on whenever I want, not just the built
in 2 timers they allow me.  Additionally, it would be nice to be able to turn the pool light 
on when the sun goes down and then turn it off maybe an hour later.  I want to be able to do 
things the controller just won't let me do.

Also, I don't want to shell out several hundred dollars for a RS232 interface that is as hard to
use as the actual controller such as the AQ-CO-SERIAL_DS.  Granted, the person that developed
that device put in a lot of work, but they had access to Goldlines documentation as well.  
Additionally, the menu directed interface of the AQ-CO-SERIAL_DS is silly.  

I have implemented a very simple input scheme of: type an 'L' and the light turns on, also, a 'l'
(lower case, notice?) will turn the light off.  A 'F' will turn on the fountain, 'f' turn it off
etc.  The filter is an 'o' for off and 'S' for high speed, 's' for low speed. The command is
sent to the pool controller and then the status is checked to see if it happened.  If it doesn't,
the code will send it again.  This operation is controlled by a timer since some of the items can
take as much as two seconds to happen.  So, not only do we have to allow for RS485 collisions,
we also have to account for the time it takes to do something.  Talk about reality programming.

There's much more description of the protocol and timing considerations on my web site at 
  desert-home.com
  
I place this code and my descriptions in the public domain and encourage people to expand on it;
I ask only that any discoveries you make be passed back to me for eventual addition to this work

Version 5 is where I added a blink to tell if the pool is talking and 
message counts for both incoming from the pool and out to the controller.
I also noticed that I hadn't put in a watchdog timer.  That's taken care
of also.  I also added the ability to force the pool to report (pool,[R,r]) and be remotely
forced to boot (pool,[B,b]).  This was to overcome something that causes it to report bad data.
I think the goldline is reporting weird and something gets out of sync.  But, the reboot seems
to fix it, so I'll go with that for now as I gather data.  It may be that I have to move the 
watchdog reset down to where I'm parsing the pool response.
*/
/* 
This has been adapted to work on an atmega2560.  The little arduino with SoftwareSerial worked
pretty well for a long time, but I got tired of the board being overwhelmed by the constant
serial input from the goldline controller.  With multiple serial ports and the hardware buffering
built into them, this has become a much more reliable device.

Version 3: Well, I finally bit the bullet and added the code to control the acid pump and sense
the float in the septic tank.  This now controls the East side of the house.
*/
#define versionID 4

#include <MemoryFree.h>
#include <avr/wdt.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <XBee.h>
#include <Button.h>

uint8_t buffer[200];
uint8_t* bufptr;

XBee xbee = XBee();
ZBRxResponse xbrx = ZBRxResponse();  // for holding the ascii traffic

// Acid Pump specific
#define acidPump 6
#define STARTBUTTON 3 //button for manually starting Acid Pump
#define ACIDLEVELPIN 4 // Where the level sensor from the reservoir is connected
#define RELAYCLOSED LOW
#define RELAYOPEN HIGH
#define PUMPTIME 20*60 //how long the pump will run after turning on.
char *acidLevel = "Low";  //Start off with the assumption that the reservoir is low
boolean pumping = 0;
// this will set the timer number to one past the highest so I can check and see if
// the offTimer has been set to go off already.  This will stop me from setting multiple
// offTimers by checking to see if there is one already and cancelling it if there is
AlarmID_t offTimer = dtNBR_ALARMS;  // just making sure it can't be allocated initially

Button startButton = Button(STARTBUTTON,PULLUP);

// Septic Tank specific
#define SEPTICPIN 5
char *septicLevel = "High";

char Dbuf[60];  // which direction is the rs485 link going (in or out)  It's half duplex
unsigned int updateCount;  // going to count XBee messages sent
unsigned int incomingCount; // and the messages from the pool
boolean sendPending = false;
int sendCount = 0;
int commandWait = 2;
#define PoolReportTime 15  // seconds between status reports
#define AcidPumpReportTime 2*60
#define SepticReportTime 3*60

#define dirPin 2
// bits for various functions
#define filterOn 0x20
#define lowSpeed 0x20  // same bit different byte
#define solarOn 0x02
#define lightOn 0x40
#define waterfallOn 0x80
#define fountainOn 0x01
// where I'll hold the data to give on request
// this was originally a series of boolean flags, that turned out
// to be harder to handle than a bit field.  If you need more bits
// at some point, use a uint16_t; that should hold you a while
uint8_t poolStatusFlag = 0;  // most recent pool status
uint8_t newStatusFlag = 0;
#define filterBit         0x80
#define filterLowSpeedBit 0x40
#define solarBit          0x20
#define lightBit          0x10
#define waterfallBit      0x08
#define fountainBit       0x04
int poolTemp = 0;
int airTemp = 0;
// Command strings to send to the pool controller
// Yes, I could construct these on the fly and been much more elegant
// for the computer purists out there.  However, this is much clearer
// and easy to understand for most folk.  Heck, you can just look at it 
// and see how the protocol works.  It was a pain calculating the 
// checksum though.
// So if you create a new one, keep the data from reading it and reuse
// the checksum value
uint8_t lightData[] =     {0x10, 0x02, 0x00, 0x83, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x98, 0x10, 0x03};
uint8_t fountainData[] =  {0x10, 0x02, 0x00, 0x83, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x9E, 0x10, 0x03};
uint8_t filterData[] =    {0x10, 0x02, 0x00, 0x83, 0x01, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x01, 0x96, 0x10, 0x03};
uint8_t waterfallData[] = {0x10, 0x02, 0x00, 0x83, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x9A, 0x10, 0x03};
uint8_t solarData[] =     {0x10, 0x02, 0x00, 0x83, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x98, 0x10, 0x03};

void showMem(){
  Serial.print("Free Memory = ");
  Serial.println(freeMemory());
}

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void setup(){
  Serial.begin(57600);
  Serial.print("Combined Pool, Acid, Septic Controller Version:  ");
  Serial.println(versionID, DEC);
  
  Serial2.begin(19200);        // Serial Port for the Pool
  pinMode(dirPin, INPUT);      // this will set the internal pull up resistor on the
  digitalWrite(dirPin, HIGH);  //digital output pin
  pinMode(dirPin, OUTPUT);
  digitalWrite(dirPin, LOW);   // to receive from rs485
  
  pinMode(13,OUTPUT);  // use this for a status LED to tell me it's alive
  
  // Specifc to the Acid Pump
  pinMode(acidPump, OUTPUT);
  digitalWrite(acidPump, RELAYOPEN);
  pinMode(ACIDLEVELPIN, INPUT);
  digitalWrite(ACIDLEVELPIN, HIGH);
  if(digitalRead(ACIDLEVELPIN) != 0)
    strcpy(acidLevel, "Low");
  else
    strcpy(acidLevel, "OK");

  Serial3.begin(58824); // this is the serial port for the XBee
  xbee.setSerial(Serial3); //hook the XBee into Serial3
  memset(buffer, 0, sizeof(buffer));
  bufptr = buffer;
  setTime(0,0,0,1,1,11);  // just so alarms work well, I don't really need the time.
  // the timealarm library is a nice way to schedule something in a few seconds or minutes
  // this can be used to send commands also.  I do this later in the code
  // to report pool status after a command is sent
  Alarm.timerRepeat(PoolReportTime, poolReport); //Show status periodically
  Alarm.timerRepeat(AcidPumpReportTime,acidPumpReport); //Acid pump report as well"
  Alarm.timerRepeat(SepticReportTime,septicReport); //and the septic tank"

  wdt_enable(WDTO_8S);   // No more than 8 seconds of inactivity
  Serial.println("done");
  
}
#define waitForFrameStart1 1
#define waitForFrameStart2 2
#define gatherData 3
#define waitForFrameEnd 4
boolean okToSend = false;

int frameStatus = waitForFrameStart1;
boolean checksumOk = false;
uint8_t* commandPtr = 0;
char requestBuffer [100];
int requestIdx = 0;
char savedc;

boolean firsttime = true;
boolean TracePackets = true;

void hangupforever(){ // this will cause it to hang up testing the watchdog timer
  while(1);
}

void loop(){
  char c;
  
  if(firsttime == true){  // the arduino has limited resouces, check them 
    showMem();            // this helps keep me under the limits
    updateCount = 0;
    incomingCount = 0;    
    firsttime = false;
//    Alarm.timerOnce(20, hangupforever); // to test the watchdog timer (debug only)
  }
  wdt_reset();                   //watchdog timer set back to zero
    // check for incoming XBee traffic
  while (Serial3.available() > 0){
    checkXbee();
  }
  // For local control of the acid pump.  It doesn't check
  // to see if the pool pump is on, since someone is standing
  // right there with it.
  if (startButton.uniquePress()){  //this prevents bounce
    if (digitalRead(acidPump) == RELAYOPEN){
      acidPumpOn();
    }
    else {
      acidPumpOff();
    }
  }
  // Now, check the level sensor in the acid tank
  if(digitalRead(ACIDLEVELPIN) == LOW)
    strcpy(acidLevel, "LOW");
  else
    strcpy(acidLevel, "OK");

  // Now, check the level sensor in the septic tank
  // The septic tank uses a normally closed switch
  if(digitalRead(SEPTICPIN) == LOW)
    strcpy(septicLevel, "OK");
  else
    strcpy(septicLevel, "HIGH");

  // made it, reset the watchdog timer
  wdt_reset();                   //watchdog timer set back to zero
  if(Serial.available()){  // extremely primitive command decoder
    c = Serial.read();
    switch(c){
      case '\r':  // carriage return, process string
        requestBuffer[requestIdx] = '\0';
//        Serial.println(requestBuffer);
        // now do something with the request string
        if(char *tmpbuf = strstr(requestBuffer,"pool,")){
          c = tmpbuf[5];
          doPoolCommand(c);
        }
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
  
  Alarm.delay(0);  //just to make the alarms work
  // the protocol has a 0x10, 0x02, data, 2 byte checksum, 0x10, 0x03 form
  // that makes it hard to parse since there are two characters to mess with
  // to add insult to injury, there is one packet type that ends with just a
  // 0x03 apparently to support an older or different display.
  // let's complicate it even more, there are packets in there that don't even 
  // come close to any of the above.  They look something like
  // "00 E0 18 80 00 E6 00 18 9E 00 E0 1E 80"  and are probably related to motor
  // control.  Since Hayward took over the company and added support for their latest
  // motors.
  if (Serial2.available()){
    c = (uint8_t)Serial2.read();
    switch( frameStatus ){
      case waitForFrameStart1:
        if(c != 0x10){
          if(TracePackets){
            Serial.println();
            Serial.print("NF - ");
            printByteData(c);   //this will print data outside the 10->03 frames
            Serial.print(" ");
          }
          break;
        }
        else {  // this is a status or command frame
          frameStatus = waitForFrameStart2;
          if(TracePackets){
            Serial.println();
            Serial.print("F  - ");
            printByteData(c);
            Serial.print(" ");
          }
          break;
        }
        Serial.println("Should not have gotten here, Start1");
        break;
      case waitForFrameStart2:
        if (c != 0x02){                   // it's not really the start of a frame
          frameStatus = waitForFrameStart1;  // Done with packet, start over
          memset(buffer, 0, sizeof(buffer));
          bufptr = buffer;
         
          if(TracePackets){
             printByteData(c);
             Serial.print(" ");
             Serial.print(" Aborted Packet");
          }
          break;
        }
        else {
          frameStatus = gatherData;
          if(TracePackets){
            printByteData(c);
            Serial.print(" ");
          }
          break;
        }
        Serial.println("Should not have gotten here, Start2");
        break;
      case gatherData:
        if (c != 0x10){
          *bufptr++ = (char)c;  // capture the characters inside a frame
          break;
        }
        else {  // nearly end of frame, check the checksum
          savedc = c;  // I know I don't need to do this, it has to be a 0x10, but ...
          int len = (int)(bufptr - buffer);
          int i = 0;
          unsigned int checksum = 0x12; // sum of 0x10 + 0x02
          for( i = 0; i < len - 2; i++){
            checksum += (unsigned int)buffer[i];
          }
          if (checksum == (unsigned int)buffer[i] * 256 + (unsigned int)buffer[i+1]){
            checksumOk = true;
          }
          else {
            checksumOk = false;
          }
          frameStatus = waitForFrameEnd;  // wait for the last 0x03
          break;
       }
        Serial.println("Should not have gotten here, Gather");
        break;
     case waitForFrameEnd:
       if(c != 0x03){
         if (savedc == 0x10){ // this packet has a 0x10 inside it
           *bufptr++ = savedc; // put it in the buffer (saved it above)
           *bufptr++ = c;  // and the character that just came in
           frameStatus = gatherData; // and collect anything else
           break;
         }
         // 
         if(TracePackets){
           Serial.print(" No 0x03 terminator");
         }
         frameStatus = waitForFrameStart1;
         memset(buffer, 0, sizeof(buffer));
         bufptr = buffer;
         break;
       }
       else {
         if(sendPending && okToSend){  //this send has to come as soon after the end of packet
                                       // as I can possibly code it.
           poolSend();
           // It can take a couple of seconds for something like the motor 
           // command to make all the way through the devices so...
           // makes you wonder why they have to run at 19.2K and create
           // so much traffic, doesn't it?
           Alarm.timerOnce(commandWait, poolActionCheck); // in a bit see if it happened
           sendPending = false;
           okToSend = false;
         }
         int len = (int)(bufptr - buffer);
         if(TracePackets){
           printFrameData(buffer,len);   // print the received data first
           printByteData((uint8_t)savedc); // Then this should be the 0x10 that got me here
           Serial.print(" ");
           printByteData(c);             // and this should be the 0x03 that ended the packet
           if(!checksumOk){
             Serial.print(" Bad Checksum");
           }
           Serial.println();
           printFrameText(buffer,len);   // the text equivalent
           }
         if(checksumOk)
           processPoolFrame(buffer, len);
         frameStatus = waitForFrameStart1;  // Done with packet, start over
         memset(buffer, 0, sizeof(buffer));
         bufptr = buffer;
         break;
       }
        Serial.println("Should not have gotten here, End");
        break;
      }
  }
}

void processPoolFrame(uint8_t* buffer, int len){

  //printFrameData(buffer,len);
  if(buffer[0] == 0x01){  // Display command
    if(buffer[1] == 0x01){  // some kind of keep-alive - ignoring for now
      okToSend= true;
      return;
    }
    //first a bit of led flashing to tell me it's alive
    if(digitalRead(13) == LOW){  // lights not already on
      digitalWrite(13,HIGH); // just to make sure I don't start too many timers
      Alarm.timerOnce(1,statusLedOff);  // this will turn the LED off
    }
    incomingCount++;                  // and keep count of them for debug
    // handle this like an interrupt.  Only set flags and deal with them after
    // the packet has been decoded.  There is not enough time to do serial prints
    // for information.  It's ok, to use them for debugging
    if(buffer[1] == 0x02){ // led display command
      poolStatusFlag = (buffer[2] & filterOn) ? poolStatusFlag | filterBit : 
                         poolStatusFlag & ~filterBit;
      // controller trying to blink filter led for low speed indication
      poolStatusFlag = (buffer[6] & lowSpeed) ? poolStatusFlag | (filterLowSpeedBit + filterBit) :
                          poolStatusFlag & ~filterLowSpeedBit;
      poolStatusFlag = (buffer[2] & solarOn)? poolStatusFlag | solarBit : 
                         poolStatusFlag & ~solarBit;
      poolStatusFlag = (buffer[2] & lightOn)? poolStatusFlag | lightBit : 
                         poolStatusFlag & ~lightBit;
      poolStatusFlag = (buffer[2] & waterfallOn)? poolStatusFlag | waterfallBit : 
                         poolStatusFlag & ~waterfallBit;
      poolStatusFlag = (buffer[3] & fountainOn)? poolStatusFlag | fountainBit : 
                         poolStatusFlag & ~fountainBit;
//      printFrameData(buffer, len);  // use this carefully, will affect the command send
    }
    if(buffer[1] == 0x03){ // Display update command
//      for ( int i = 0; i < 32; i++){  // use this to show text intended for display
//        Serial.print(buffer[i+2]);
//      }
//      Serial.println();
      if (strncmp((char *)buffer+2,"Pool Temp",9) == 0){
        poolTemp = atoi((char *)buffer+12);
      }
      if (strncmp((char *)buffer+2,"Air Temp",8) == 0){
        airTemp = atoi((char *)buffer+11);
      }
    }
  }
  // Motor response packets
  else if (buffer[0] == 0x00 && buffer[1] == 0x0c){
    Serial.println();
    Serial.print("Motor running at ");
    Serial.print(((long)3450 * buffer[4]) / 100);
    Serial.print(" and using ");
    printByteData(buffer[5]);
    printByteData(buffer[6]);
    Serial.print(" Watts");
    Serial.println();
  }
  else if (buffer[0] == 0x00 && buffer[1] == 0x83){
    /* 
       0x00, 0x83 is the command string sent by the wireless controller
       below are the bit patterns from the various buttons.  These are repeated
       twice in the message so 00 83 00 01 00 00 00 01 00 00 would be a light button
       80 00 00 00   filter button
       20 00 00 00   plus sign  (- doesn't send sometimes)
       40 00 00 00   pool button
       01 00 00 00   right arrow
       02 00 00 00   menu button
       04 00 00 00   left arrow
       08 00 00 00   System off button
       00 01 00 00   Light  
       00 02 00 00   Waterfall    (Aux 1)
       00 04 00 00   Fountain     (Aux 2)
       00 00 01 00   Solar Heater (Valve 3)
       00 00 02 00   Unused switch on bottom
       00 00 04 00   Unused switch in middle
    */
//    printFrameData(buffer, len); // this will print command buffers outgoing from remote
  }
}

void poolSendFlag(){  // this flag routine tells the send code to go ahead
  sendPending = true;
}

void poolActionCheck(){
  
  if ((poolStatusFlag | solarBit) != (newStatusFlag | solarBit)){// solar is thermostat controlled
                                                                 // don't check it
    // this is if the command didn't work
    if (sendCount++ >= 4){  // it has 5 chances to succeed
      commandPtr = 0;  // command didn't work, user will have to retry
      sendCount = 0;
      return;
    }
    poolSendFlag();  // make it try again
    // This will now loop around through the frame handling and resend
    // which will set another timer to come back here and check again. 
    // and so forth until the action has completed or failed.
  }
  else {  // command succeeded
    poolReport();
    commandPtr = 0;  //command succeeded, cancel it
    sendCount = 0;
  }
}

void poolSend(){
// This is rs485 and there is no collision protection.  It doesn't seem like the protocol 
// goldline uses has an addressing scheme to enable the various possible control panels either
// However, the commands have to be sent as SOON as
// possible after the end of a packet.  They must be only accepting commands for a timed period 
// because I don't get on, off, on, off, types of reactions.  Ten commands only results in one
// action if they happen close enough together.

    digitalWrite(dirPin, HIGH); // to send rs485
    for(int i = 0; i < sizeof(lightData); i++){
     Serial2.write(commandPtr[i]);
    }
    delay(500); // had to add this when using mega2560
    digitalWrite(dirPin, LOW); // to receive from rs485
    // the time in the advisory is to let you know if the alarm to wait is working
    sprintf(Dbuf,"Command Send ******%lu********wait=%d",now(),commandWait);
    Serial.println(Dbuf);
}

void statusLedOff(){
  digitalWrite(13,LOW);
}

void printFrameData(uint8_t* buffer, int len){
  
  // there's lots of nulls in there, can't just print it
  // so I print the hex values then the ascii below
  // so you can see the text as it flys by
  int i = 0;
  while(i < len){
    printByteData(buffer[i++]);
    Serial.print(" ");
  }
}

void printFrameText(uint8_t* buffer, int len){
  int i = 0;
  Serial.print("           ");
  while(i < len){
    Serial.print(" ");
    if (isprint(buffer[i]))
      Serial.write(buffer[i]);
    else
      Serial.print(" ");
    Serial.print(" ");
    i++;
  }
}

// routine to take binary numbers and show them as two hex digits
void printByteData(uint8_t Byte){
  Serial.print((uint8_t)Byte >> 4, HEX);
  Serial.print((uint8_t)Byte & 0x0f, HEX);
}
