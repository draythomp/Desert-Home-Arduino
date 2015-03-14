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
Reading a tmp36 chip to get temperature.
These devices used to be really flakey until I discovered what I was
doing wrong. Details are on my blog. Even after I got it to settle
down and work correctly there was the occasional outlying reading.
These weren't bad, just a degree or two up or down, and they're 
easy to correct. I read the sensor 15 times, sort it, and then
take the middle five readings and average them. This gets the odd
occasional reading at one end or the other of the array and picking
the middle settles the reading right down.
*/
#define READSIZE 15

float readTemp2(){
  int readings[READSIZE];
  int reading=0;
  
  for (int i = 0; i < READSIZE; i++){
    readings[i] = analogRead(tmpInput);
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
  float tempC = (voltage - 0.5) * 100;
  float tempF = (tempC * 9.0 / 5.0) + 32.0;
  return(tempF);
}
/*
These two routines use the atmega328 built in thermometer and VCC measurement techniques
This way I can get the supply voltage to see how the batteries are doing, as well as grab
the temperature of the chip.  It may need calibration, but it's free
*/
float readTemp(){
  long result; // Read temperature sensor against 1.1V reference
  ADMUX = _BV(REFS1) | _BV(REFS0) | _BV(MUX3);
  delay(20); // Wait for Vref to settle - 2 was inadequate
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = (result - 125) * 1075;
  return (result / 10000.0) * 1.8 + 32.0; // I want degrees F
}

float readVcc(){
  long resultVcc; // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(20); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  resultVcc = ADCL;
  resultVcc |= ADCH<<8;
  resultVcc = 1126400L / resultVcc; // calculate AVcc in mV
  return (resultVcc / 1000.0); // but return volts
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

