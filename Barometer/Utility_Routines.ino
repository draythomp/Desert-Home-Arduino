// this tab holds the various routines that handle things
// I don't want to clutter up the main module with all these little 
// items

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

// Special code for a watchdog timer.
#define TIMEOUTPERIOD 10000             // You can make this time as long as you want,
                                       // it's not limited to 8 seconds like the normal
                                       // watchdog
#define doggieTickle() resetTime = millis();  // This macro will reset the timer.
unsigned long resetTime = 0;
void watchdogSetup()
{
cli();  // disable all interrupts
wdt_reset(); // reset the WDT timer
MCUSR &= ~(1<<WDRF);  // because the data sheet said to
/*
WDTCSR configuration:
WDIE = 1 Interrupt Enable
WDE = 1  Reset Enable - I won't be using this on the 2560
WDP3 = 0 For 1000ms Time-out
WDP2 = 1 bit pattern is 
WDP1 = 1 0110  change this for a different
WDP0 = 0 timeout period.
*/
// Enter Watchdog Configuration mode:
WDTCSR = (1<<WDCE) | (1<<WDE);
// Set Watchdog settings: interrupte enable, 0110 for timer
WDTCSR = (1<<WDIE) | (0<<WDP3) | (1<<WDP2) | (1<<WDP1) | (0<<WDP0);
sei();
}

ISR(WDT_vect) // Watchdog timer interrupt.
{ 
  if(millis() - resetTime > TIMEOUTPERIOD){
    resetFunc();
  }
}

// returns the day of year 1-365 or 366 so Feb 29th is the 60th day.
int dayOfYear(){
  return( 1 + (g(year(), month(), day()) - g(year(), 1, 1)) );
}
// this converts to a day number allowing for leap years and such
int g(int y, int m, int d){
  unsigned long yy, mm, dd;
  dd = d;
  mm = (m + 9) % 12;
  yy = y - mm/10;
  return (int)(365*yy + yy/4 - yy/100 + yy/400 + (mm * 306 + 5)/10 + (dd - 1));
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

// Utility routines for printing packets when needed for debug
void printByteData(uint8_t Byte){
  Serial.print((uint8_t)Byte >> 4, HEX);
  Serial.print((uint8_t)Byte & 0x0f, HEX);
}
// these routines are just to print the data with
// leading zeros and allow formatting such that it 
// will be easy to read.
void print32Bits(uint32_t dw){
  print16Bits(dw >> 16);
  print16Bits(dw & 0xFFFF);
}

void print16Bits(uint16_t w){
  print8Bits(w >> 8);
  print8Bits(w & 0x00FF);
}
  
void print8Bits(byte c){
  uint8_t nibble = (c >> 4);
  if (nibble <= 9)
    Serial.write(nibble + 0x30);
  else
    Serial.write(nibble + 0x37);
        
  nibble = (uint8_t) (c & 0x0F);
  if (nibble <= 9)
    Serial.write(nibble + 0x30);
  else
    Serial.write(nibble + 0x37);
}

