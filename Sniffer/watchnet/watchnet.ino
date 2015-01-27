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

byte Pbuf[500];
char Dbuf[50];

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
  Serial.begin(115200);
// Init the chibi stack
  chibiInit();
  Serial.println(F("I'm alive *****************"));
  unsigned int choice = 0;
  showMem();
  Serial.println(F("Any character to start"));

  while(1){
    if(Serial.available() != 0){
      char c = Serial.read();
      break;
     }
  }
  Serial.print(F("Starting Radio, Result = "));
  Serial.println(chibiSetChannel(13),DEC);
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
    senderAddress = chibiGetSrcAddr();
    if (len > 0){
      Serial.print(F("From:"));
      Serial.print(senderAddress,HEX);
      Serial.print(F(" Len: "));
      Serial.println(len);
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

