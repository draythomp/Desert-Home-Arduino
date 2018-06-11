// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print(F("0"));
    Serial.print(deviceAddress[i], HEX);
  }
}

// main function to print information about a device
float getTemp(DeviceAddress deviceAddress)
{
  Serial.print(F("Reading device Address: "));
  printAddress(deviceAddress);
  Serial.print(F(" "));
  float tempF = sensors.getTempF(deviceAddress);
  Serial.print(F(" Temp F: "));
  Serial.print(tempF);
  Serial.println();
  return(tempF);
}

void showMem(){
  uint8_t * heapptr, * stackptr;
  
  Serial.print(F("Mem = "));
  stackptr = (uint8_t *)malloc(4);   // use stackptr temporarily
  heapptr = stackptr;                // save value of heap pointer
  free(stackptr);                    // free up the memory again (sets stackptr to 0)
  stackptr =  (uint8_t *)(SP);       // save value of stack pointer
  Serial.println(stackptr - heapptr);
}

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void pass(){ // This is like the python pass statement. (look it up)
    return;
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

// water heater control commands
void waterHeaterOn(){
  digitalWrite(waterHeater,HIGH);
  Report();
}
void waterHeaterOff(){
  digitalWrite(waterHeater,LOW);
  Report();
}
