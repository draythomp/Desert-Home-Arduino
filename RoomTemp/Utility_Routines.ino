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
/*
Yep, this is all there is to reading a 18B20 temperature sensor

Well, of course there's two libraries involved with hundreds of lines of code to 
support this, but who's counting?
*/

float readTemp(){
  sensors.requestTemperatures(); // Send the command to get temperatures
  float tempF = sensors.getTempFByIndex(0);
  return(tempF);
}

#define READSIZE 15

float readVcc(){
  int readings[READSIZE];
  int reading=0;
  
  // First, give it a dealy to let the chip settle
  // from being asleep
  delay(10);
  for (int i = 0; i < READSIZE; i++){
    readings[i] = analogRead(voltagePin);
  }
  // Now sort the list to put the outliers at the beginning and end
  sort(readings, READSIZE);
  // grab the middle 5 and average them
  for (int i = READSIZE / 2 - 2; i < READSIZE / 2 + 3 ; i++){
    reading +=(readings[i]);
  }
  reading /= 5;
  //reading = analogRead(tmpInput);
  float voltage =  (reading * 1.1) / 1024;
  return(voltage * 12.0);
}

void sort(int a[], int size) {
  for(int i=0; i<(size-1); i++) {
    for(int o=0; o<(size-(i+1)); o++) {
      if(a[o] > a[o+1]) {
        int t = a[o];
        a[o] = a[o+1];
        a[o+1] = t;
      }
    }
  }
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

