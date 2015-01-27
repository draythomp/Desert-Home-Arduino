/*
This sketch enables promiscuous mode on the Freakduino boards and then
filters by device address to watch a particular node.

I had to change the buffer size in the chibi library for promiscuous mode
because, being set to 1024 with another buffer for the packet to be manipulated
caused the board to run out of RAM.  Just go into the chibi library and 
find the receive buffer in chb_buf.h and set it to what you want.  I used the
value 500, and it seems to work fine

Before using this sketch, also go into the chibiUsrCfg.h file and 
enable promiscuous mode. To do this, change the definition:

#define CHIBI_PROMISCUOUS 0
to 
#define CHIBI_PROMISCUOUS 1

When not using promiscuous mode, disable this setting by changing
it back to 0.
*/

#include <chibi.h>
#include <avr/pgmspace.h>
#include <MemoryFree.h>

/*
Addresses of devices in my network
                    Long     Short
Garage Controller 407A 38AF  A019 
Power Monitor     4031 566B  CD6B
Controller        4055 B0A6  97FA
Status Display    406F B6B5  9BE8
Pool Controller   4079 B2BD  9370
Temp1             406F 7FAC  1B1E
House Clock       4055 B094  0
Acid Pump         406F B7AF  7D58
*/
char *deviceName[] = {"Garage Controller",
                       "Power Monitor",
                       "Controller",
                       "Status Display",
                       "Pool Controller",
                       "Temp1",
                       "House Clock",
                       "Acid Pump"};
uint16_t deviceAddress[] = {0xA019, 
                         0xCD6B,
                         0x97FA,
                         0x9BE8,
                         0x9370,
                         0x1B1E,
                         0x0,
                         0x7D58};
uint16_t listenTo;
byte Pbuf[500];
char Dbuf[50];
int longest = 0;

void showMem(){
  strcpy_P(Dbuf,PSTR("Mem = "));
  Serial.print(Dbuf);
  Serial.println(freeMemory());
}

/**************************************************************************/
// Initialize
/**************************************************************************/
void setup()
{  
  // Init the chibi stack
  Serial.begin(115200);
  chibiInit();
  strcpy_P(Dbuf,PSTR("\n\r***************I'm alive**************"));
  Serial.println(Dbuf);
  unsigned int choice = 0;
  Serial.println("Pick a device to listen to");
  for(int i = 0; i < sizeof(deviceName)/sizeof(deviceName[0]); i++){
    Serial.print((char)(i + 'A'));
    Serial.print(" - ");
    Serial.println(deviceName[i]);
  }
  Serial.print("> ");
  // this only accepts a single character for the device choice.
  // that's because the chibi board runs at 8MHz on the internal clock and
  // internal clocks aren't accurate.  This means the higher baud rates sometimes
  // have problems.  When I worked on this I couldn't get the board to accept more
  // than one character correctly at baud rates over 57K.  Works fine at 9600,
  // but I want to output the data far faster than that.
  while(1){
    if(Serial.available() != 0){
      char c = Serial.read();
      Serial.write(c);
      Serial.println();
      choice = toupper(c) - 'A';
      if (choice < sizeof(deviceName)/sizeof(deviceName[0]))
        break;
      else {
        Serial.println(" oops, try again");
        Serial.print("> ");
      }
    }
  }
  listenTo = deviceAddress[choice];
  strcpy_P(Dbuf,PSTR("Starting Radio, Result = "));
  Serial.print(Dbuf);
  Serial.println(chibiSetChannel(12),DEC);
  strcpy_P(Dbuf,PSTR("Listening to "));
  Serial.print(Dbuf);
  Serial.print(deviceName[choice]);
  strcpy_P(Dbuf,PSTR(" at address "));
  Serial.print(Dbuf);
  Serial.println(listenTo, HEX);
  showMem();
}

/**************************************************************************/
// Loop
/**************************************************************************/
void loop()
{
  // Check if any data was received from the radio. If so, then handle it.
  if (chibiDataRcvd() == true)
  {  
    int len;
    uint16_t senderAddress;
    
    // send the raw data out the serial port in binary format
    len = chibiGetData(Pbuf);
    if (len > longest)
      longest = len;
    senderAddress = chibiGetSrcAddr();
    if (senderAddress == listenTo && len > 0){
      strcpy_P(Dbuf,PSTR("From: "));
      Serial.print(Dbuf);
      Serial.print(senderAddress,HEX);
      strcpy_P(Dbuf,PSTR(" Len: "));
      Serial.print(Dbuf);
      Serial.print(len);
      strcpy_P(Dbuf,PSTR(" Longest: "));
      Serial.print(Dbuf);
      Serial.println(longest);
      for (int i= 0; i < len; i++){
        uint8_t nibble = (uint8_t) (Pbuf[i] >> 4);
        if (nibble <= 9)
          Serial.write(nibble + 0x30);
        else
          Serial.write(nibble + 0x37);
        
        nibble = (uint8_t) (Pbuf[i] & 0x0F);
        if (nibble <= 9)
          Serial.write(nibble + 0x30);
        else
          Serial.write(nibble + 0x37);
        Serial.print(' ');
      }
      Serial.println();
      for (int i= 0; i < len; i++){
        uint8_t nibble = (uint8_t) (Pbuf[i] >> 4);
        Serial.write(' ');
        if (iscntrl(Pbuf[i]))
          Serial.write(' ');
        else
          Serial.write(Pbuf[i]);
        Serial.write(' ');
      }
      Serial.println();
    }
  }
}

