/* Device to get time from satellite and supply it via web
 * to any device that needs it.  I used source and ideas from 
 * all over the web, and frankly, I've forgotten everywhere I 
 * gathered from.  However, thank you all for the help.
 *
 * Open source rules!
 * This is open source, have fun.
 */

#include <string.h>
#include <ctype.h>
#include <SoftwareSerial.h> // this is New softwareserial 10C
#include <Time.h>
#include <MemoryFree.h>
#include <avr/pgmspace.h>
#include <Ethernet.h>
#include <SPI.h>
#include <EEPROM.h>
#include "template.h"  //I had to add this because the compiler changed (it's in a tab)


#define GPSrxPin 2
#define GPStxPin 3
#define LCDrxPin 4
#define LCDtxPin 5
#define XBeerxPin 6
#define XBeetxPin 7
#define rxSense 8
#define tzOffset -7
int ledPin = 13;                  // LED test pin
char byteGPS=-1;
char lineGPS[100];
char general[100],Sgeneral[100];
time_t tNow;
struct saved_s
{
  long magic_number;
  time_t saved_time;
} saved, tempsaved;

void(* resetFunc) (void) = 0; //declare reset function @ address 0

int count=0;
SoftwareSerial GPSserial = SoftwareSerial(GPSrxPin, GPStxPin);
SoftwareSerial LCDserial = SoftwareSerial(LCDrxPin, LCDtxPin);
SoftwareSerial XBeeserial = SoftwareSerial(XBeerxPin, XBeetxPin);

// MAC address and IP address for controller below.
// The IP address will be dependent on your local network:
byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEF };
byte ip[] = { 192,168,0,204 };
byte gateway_ip[] = {192,168,0,1};	// router or gateway IP address
byte subnet_mask[] = {255,255,255,0};	// subnet mask for the local network
byte thing[] = {192,168,0,1};           // address of my router
EthernetServer Control(80);                     //webserver on port 80
EthernetClient Test;                 //talk to something to be sure network is working

int get2num(char *buf){
  int val = -1;

  if(isdigit(buf[0]))
    val = (buf[0] - 0x30);
  if(isdigit(buf[1]))
    val = val * 10 + (buf[1] - 0x30);
  if(val != -1)
    return(val);
  else
    return(-1);
}
void showMem(){
  strcpy_P(general,PSTR("Mem = "));
  Serial.print(general);
  Serial.println(freeMemory());
}
//
//  Sparkfun LCD display routines
//
void LCDgoto(int position) { //position = line 1: 0-15, line 2: 16-31, 31+ defaults back to 0
  if (position<16)
  { 
    LCDserial.write(0xFE);              //command flag
    LCDserial.write(position+128);    //position
  }
  else if (position<32){
    LCDserial.write(0xFE);   //command flag
    LCDserial.write(position+48+128);          //position 
  } 
  else 
  {
    LCDgoto(0); 
  }
}

void LCDline1()
{
  LCDserial.write(0xFE);   //command flag
  LCDserial.write(128);    //position to line 1
}

void LCDline2()
{
  LCDserial.write(0xFE);   //command flag
  LCDserial.write(192);    //position to line 2
}
void LCDclear(){
  LCDserial.write(0xFE);   //command flag
  LCDserial.write(0x01);   //clear command.
  delay(100);
}

// Displays the time on the LCD screen
void showTime()
{
  tNow = now();
  LCDserial.begin(9600);
  LCDline1();
  strcpy_P(Sgeneral,PSTR("%2d:%02d:%02d %2s"));
  sprintf(general,Sgeneral,hourFormat12(tNow),minute(tNow),second(tNow),
      isAM(tNow)?"AM":"PM");
  LCDserial.print(general);
  LCDline2();
  strcpy_P(Sgeneral,PSTR("%2d/%02d/%02d"));
  sprintf(general,Sgeneral,month(tNow),day(tNow),year(tNow));
  LCDserial.print(general);
}

boolean ethernetOK(){
    int cnt = 10;
    int result = 0;
    while (cnt-- != 0){  // simply count the number of times the light is on in the loop
      result += digitalRead(rxSense);
      delay(50);
    }
    Serial.println(result,DEC);
    if (result >=6)     // experimentation gave me this number YMMV
      return(true);
    else
      return(false);
}

void ethernetReset(){
  bool rxState;
  int cnt = 10, result;
  
  pinMode(rxSense, INPUT);  //for stabilizing the ethernet board
  pinMode(A1,INPUT);
  while(1){
    digitalWrite(A1, HIGH);
    pinMode(A1, OUTPUT);
    digitalWrite(A1, LOW);  // ethernet board reset
    delay(100);
    digitalWrite(A1, HIGH);
    delay(2000);
    if (ethernetOK()){
      Ethernet.begin(mac, ip, gateway_ip, subnet_mask);
      delay(1000);  // wait a bit for the card to stabilize
      return;
    }
    delay(50);
  }
}


boolean checkNetwork(){
  int waitcnt = 0;
  boolean TestOK = false;
  
  while(1){
    if (!TestOK){
      if (Test.connect(thing, 80)) {
        TestOK = true;
        Test.println("GET / HTTP/1.1\r\n");
      }
      else{
        Test.flush();
        Test.stop();
        return(false);
      }
    }
    delay(100);
    if(waitcnt++ > 10){
      Test.flush();
      Test.stop();
      return(false);
    }
    if (Test.available()){
      while (Test.available()) {
        char c = Test.read();
//        Serial.print(c);
      }
      Test.flush(); //suck out any extra chars  
      Test.stop();
      while (Test.status() != 0){
        delay(5);
      }
      return(true);
    }
  }
}

time_t getGPStime(){
  boolean timeset = false;
  char *buf, *buf2;
  tmElements_t sat_tm; 

  GPSserial.begin(4800);
  strcpy_P(general,PSTR("Time Syncronize to "));
  Serial.print(general);
  count=0;
  while(!timeset){
    digitalWrite(ledPin, HIGH);
    if (GPSserial.available() > 0){
      byteGPS=GPSserial.read();         // Read a byte of the serial port
 //     Serial.print(byteGPS);            //Debugging
      lineGPS[count++] = byteGPS;       // If there is serial port data, it is put in the buffer
      if (byteGPS == '\r'){             // If the received byte is a return, end of transmission
        digitalWrite(ledPin, LOW);
        lineGPS[count] = '\0';          //null at the end of line
        if (strstr_P(lineGPS, PSTR("$GPRMC")) != 0){
          // first make sure the check sum is ok
          if(!validateChecksum(lineGPS)){
            strcpy_P(general,PSTR("Checksum Bad"));
            Serial.println(general);
            return(0);
          }
          buf = strtok_r(lineGPS, ",",&buf2);   //First comma
          buf = strtok_r(NULL, ",",&buf2);      //Second comma
          sat_tm.Hour = get2num(buf);                //get hour
          if (sat_tm.Hour < 0 || sat_tm.Hour > 23){
            strcpy_P(general,PSTR("Hour problem"));
            Serial.println(general);
            return(0);  //time was invalid, try again
          }
          buf +=2;
          sat_tm.Minute = get2num(buf);               //minute
          if (sat_tm.Minute < 0 || sat_tm.Minute > 59){
            strcpy_P(general,PSTR("Minute problem"));
            Serial.println(general);
            return(0);  //time was invalid, try again
          }
          buf +=2;
          sat_tm.Second = get2num(buf);               //second
          if (sat_tm.Second < 0 || sat_tm.Second > 59){
            strcpy_P(general,PSTR("Second problem"));
            Serial.println(general);
            return(0);  //time was invalid, try again
          }
          for(int i=0;i<8;i++)
            buf = strtok_r(NULL, ",",&buf2);    //skip 8 more commas
          sat_tm.Day = get2num(buf);                //day
          if (sat_tm.Day < 1 || sat_tm.Day >31){
            strcpy_P(general,PSTR("Day problem"));
            Serial.println(general);
            return(0);  //time was invalid, try again
          }
          buf +=2;
          sat_tm.Month = get2num(buf);                //month
          if (sat_tm.Month < 1 || sat_tm.Month > 12){
            strcpy_P(general,PSTR("Month problem"));
            Serial.println(general);
            return(0);  //time was invalid, try again
          }
          buf +=2;
          sat_tm.Year = get2num(buf) + 30;    // 2 digit year
          // so 2011 will become 41  (11 + 30) because tm_year 
          // is  an offset from 1970
          if (sat_tm.Year < 41){
            Serial.println(sat_tm.Year, DEC);
            strcpy_P(general,PSTR("Year problem"));
            Serial.println(general);
            return(0);  //time was invalid, try again
          }
          tNow = makeTime(sat_tm);
          tNow = tNow +(tzOffset * 3600);
          if (tNow < saved.saved_time){
            strcpy_P(general, PSTR("FAILED Low"));
            Serial.println(general);
            return(0);  //time was invalid, try again
          }
          if (tNow > (saved.saved_time + SECS_PER_YEAR)){
            strcpy_P(general, PSTR("FAILED HIGH"));
            Serial.println(general);
            return(0);  //time was invalid, try again
          }
          setTime(tNow);
          strcpy_P(Sgeneral,PSTR("%02d:%02d:%02d %02d/%02d/%04d"));
          sprintf(general, Sgeneral,
            hour(tNow), minute(tNow), second(tNow), month(tNow),
            day(tNow), year(tNow));
          Serial.println(general);
          timeset = true;
          return(now());
        }
        count=0; 
      }
    }
    if(count==99)
      count = 0;
  }
}

// Validates the checksum on an (for instance NMEA) string
// Returns 1 on valid checksum, 0 otherwise
int validateChecksum(char *buffer) {
  char gotSum[3];
  gotSum[0] = buffer[strlen(buffer) - 3];
  gotSum[1] = buffer[strlen(buffer) - 2];
  gotSum[2] = 0; // null at the end
  // Check that the checksums match up
  if ((int)strtol(gotSum,0,16) == getCheckSum(buffer)) 
    return 1;
  else 
    return 0;
}

// Calculates the checksum for a given string
// returns as integer
int getCheckSum(char *string) {
  int i;
  int XOR;
  char c;
  
  // Calculate checksum ignoring any $'s in the string
  for (XOR = 0, i = 0; i < strlen(string); i++) {
    c = string[i];
    if (c == '*') break;
    if (c != '$' && isprint(c)) XOR ^= c; //just in case a LF or CR slipped in
  }
  return XOR;
}
void eepromadjust(){
  EEPROM_readAnything(0, tempsaved); // get saved settings
  if (saved.saved_time == (time_t) 0) // never save a zero time.
    return;
  if(saved.saved_time != tempsaved.saved_time){
      EEPROM_writeAnything(0, saved);
      strcpy_P(general, PSTR("EEPROM parameters updated"));
      Serial.println(general);
    }
}

void setup() {
  Serial.begin(57600);
  delay(3000); // this allows for the sparkfun logo to time out.
  // Previously saved time for reasonableness comparison
  EEPROM_readAnything(0, saved); // get saved settings
  if (saved.magic_number != 1234){ //this will set it up for very first use
    saved.magic_number = 1234;
    saved.saved_time = 1296565408;
    EEPROM_writeAnything(0, saved);
  }
  Serial.print("Saved time is ");
  strcpy_P(Sgeneral,PSTR("%02d:%02d:%02d %02d/%02d/%04d"));
  sprintf(general, Sgeneral,
         hour(saved.saved_time), minute(saved.saved_time), second(saved.saved_time), month(saved.saved_time),
         day(saved.saved_time), year(saved.saved_time));
  Serial.println(general);
  pinMode(ledPin, OUTPUT);       // Initialize LED pin
  GPSserial.begin(4800);
  strcpy_P(general,PSTR("Hello World"));
  Serial.println(general);
  showMem();
  LCDserial.begin(9600);
  LCDserial.write(0x7C);   // Intensity to max
  LCDserial.write(157);
  delay(100);
  LCDserial.write(0xFE);   //command flag
  LCDserial.write(0x01);   //clear command.
  LCDline1();
  strcpy_P(Sgeneral,PSTR("Get GPS Time ..."));
  LCDserial.write(Sgeneral);
  
  setSyncInterval(30 * 60);  //update time every thirty minutes
  while(timeStatus() == timeNotSet){
    setSyncProvider(getGPStime); //this will immediately go get the time
  }
  
  XBeeserial.begin(9600);  //For XBee time broadcast
  delay(100);
  // start the Ethernet connection:
  // first, reset the board and make sure it intialized correctly
  strcpy_P(Sgeneral,PSTR("Ethernet ..."));
  LCDline2();
  LCDserial.write(Sgeneral);
  ethernetReset();
  saved.saved_time = now();
  eepromadjust();  // every time it boots successfully, set a new standard

  Control.begin(); // start the web control server
  LCDclear();
  showTime();
}

boolean firsttime = true;
int seconds = 0;

void loop() {
  char netbuf[50];
  int index = 0;

  if(firsttime == true){  // keep from running out of memory!!
    showMem();
    firsttime = false;
  }
  time_t curtime = now();
  if (seconds != second(curtime)){
    showTime();
    seconds = second(curtime);
    if((minute(curtime) % 15 == 0) && (second(curtime) == 0)){
      strcpy_P(Sgeneral, PSTR("%02d:%02d:%02d"));
      sprintf(general, Sgeneral, hour(curtime),minute(curtime),second(curtime));
      Serial.println(general);
    }
    if(day(curtime) % 2 == 0 && hour(curtime) == 1 && minute(curtime) == 1 && second(curtime) == 0){ // every couple of days update 
      saved.saved_time = now();
      eepromadjust();         // the saved time for checks
      strcpy_P(general,PSTR("Updated saved time"));
      Serial.println(general);
    }
    if(second(curtime) % 60 == 0){
      if (!checkNetwork()){
        strcpy_P(Sgeneral, PSTR("Network failure at %02d:%02d:%02d"));
        sprintf(general, Sgeneral, hour(curtime),minute(curtime),second(curtime));
        Serial.println(general);
        ethernetReset();
        Control.begin(); // start the web control server
      }
    }
    if(second(curtime) %10 == 0){
      strcpy_P(Sgeneral, PSTR("Time,%lu,%02d:%02d:%02d,%02d/%02d/%04d\r"));
      sprintf(general, Sgeneral, (unsigned long)curtime,
                         hour(curtime), minute(curtime), second(curtime),
                         month(curtime), day(curtime), year(curtime));
      XBeeserial.print(general);
    }
  }
   EthernetClient ControlClient = Control.available();
   if (ControlClient) {
    // an http request ends with a blank line
    while (ControlClient.connected()) {
      if (ControlClient.available()) {
        char c = ControlClient.read();
        // if you've gotten to the end of the line (received a newline
        // character) you can send a reply
        if (c != '\n' && c!= '\r') {
          netbuf[index++] = c;
          if (index >= sizeof(netbuf)) // max size of buffer
            index = sizeof(netbuf) - 1;
          continue;
        }
        netbuf[index] = 0;
        Serial.println(netbuf);
        // standard http response header
        strcpy_P(general, PSTR("HTTP/1.1 200 OK"));
        ControlClient.println(general);
        strcpy_P(general, PSTR("Content-Type: text/html"));
        ControlClient.println(general);
        ControlClient.println();
        if (strstr_P(netbuf,PSTR("GET / ")) != 0){ //for people
          strcpy_P(general, PSTR("<html><body><h1>"));
          ControlClient.print(general);
          strcpy_P(general, PSTR("Home Satellite Clock<br>"));
          ControlClient.print(general);
          tNow = now();
          strcpy_P(Sgeneral,PSTR("%d:%02d:%02d %d/%d/%d"));
          sprintf(general,Sgeneral,hour(tNow),minute(tNow),second(tNow),month(tNow),day(tNow),year(tNow));
          ControlClient.println(general);
          strcpy_P(general, PSTR("<br></h1></body></head></html>"));
          ControlClient.println(general);
          break;
        }
        else if (strstr_P(netbuf,PSTR("GET /thetime")) != 0){ //for people
          tNow = now();
          strcpy_P(Sgeneral,PSTR("%d:%02d:%02d"));
          sprintf(general,Sgeneral,hour(tNow),minute(tNow),second(tNow));
          ControlClient.println(general);
          break;
        }
        else if (strstr_P(netbuf,PSTR("GET /thedate")) != 0){ //for people
          tNow = now();
          strcpy_P(Sgeneral,PSTR("%d/%d/%d"));
          sprintf(general,Sgeneral,month(tNow),day(tNow),year(tNow));
          ControlClient.println(general);
          break;
        }
        else if (strstr_P(netbuf,PSTR("GET /time_t")) != 0){ //for other machines
          strcpy_P(Sgeneral,PSTR("%lu"));
          sprintf(general,Sgeneral,(unsigned long)now());
          ControlClient.println(general);
          break;
        }
        else if (strstr_P(netbuf,PSTR("GET /status")) != 0){ //for machines, comma separated
          tNow = now();
          strcpy_P(Sgeneral,PSTR("%d:%02d:%02d,%d/%d/%d"));
          sprintf(general,Sgeneral,hour(tNow),minute(tNow),second(tNow),
                month(tNow),day(tNow),year(tNow));
          ControlClient.println(general);
          break;
        }
        else if (strstr_P(netbuf,PSTR("GET /reset")) != 0){ //for other machines
          strcpy_P(general,PSTR("Reset default"));
          Serial.println(general);
          saved.saved_time = 1296565408;
          EEPROM_writeAnything(0, saved);
          resetFunc();  // general troubleshooting
          break;        // won't ever get here
        }
        else{
          strcpy_P(general,PSTR("command error"));
          ControlClient.println(general);
          break;
        }
      }
    }
   }
   ControlClient.stop();
}

