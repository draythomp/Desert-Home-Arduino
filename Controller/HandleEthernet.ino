/*
    macro to compare two ip addresses using the netmask
 Basically, it compares the addresses using only the zeros
 in the net mask.  This will tell you if the incoming
 machine is local to your network.
 */
#define maskcmp(addr1, addr2, mask) \
(((((uint16_t *)addr1)[0] & ((uint16_t *)mask)[0]) == \
             (((uint16_t *)addr2)[0] & ((uint16_t *)mask)[0])) && \
            ((((uint16_t *)addr1)[1] & ((uint16_t *)mask)[1]) == \
             (((uint16_t *)addr2)[1] & ((uint16_t *)mask)[1])))

void ethernetReset(){

  pinMode(rxSense, INPUT);  //for stabilizing the ethernet board
  pinMode(ethernetResetPin,INPUT);
  while(1){
    digitalWrite(ethernetResetPin, HIGH);
    pinMode(ethernetResetPin, OUTPUT);
    digitalWrite(ethernetResetPin, LOW);  // ethernet board reset
    delay(100);
    digitalWrite(ethernetResetPin, HIGH);
    delay(5000);
    // now, after the reset, check the rx pin for constant on
    if (ethernetOk())
      return;
    delay(100);
  }
}

boolean ethernetOk(){
  int cnt = 10, result;

  cnt = 10;
  result = 0;
  while (cnt-- != 0){  // simply count the number of times the light is on in the loop
    result += digitalRead(rxSense);
    delay(50);
  }
  Serial.print(F("Ethernet setup result "));
  Serial.println(result,DEC);
  if (result >=6)      // experimentation gave me this number YMMV
    return(true);
  return(false);
}
      
boolean isLocal(EthernetClient incoming){
  /*
    this depends on code changes to the ethernet library 
    as detailed in http://arduino.cc/forum/index.php/topic,54378.0.html
   */
  uint8_t clientIp[4];
  uint8_t clientMac[6];

  incoming.getRemoteIP_address(&clientIp[0]);
  incoming.getRemoteMAC_address(&clientMac[0]);
  /*  This is for debugging */
  Serial.print(F("Incoming IP is: "));
  for (int n = 0; n<4; n++){
    Serial.print((int)clientIp[n]);
    Serial.print(".");
  }
  Serial.print(F(" MAC is:"));
  for (int n = 0; n<6; n++){
    Serial.print(clientMac[n] >> 4, HEX);
    Serial.print(clientMac[n] & 0x0f, HEX);
    Serial.print(".");
    // my router has two mac addresses
    //00.1F.B3.79.B3.69 seen inside
    //00.1F.B3.79.B3.68 seen outside
    //So, if you're coming from outside, the .69 address will
    //be seen.  Coming from inside you will see the actual
    //mac address.  That kinda sucks for using the mac address to 
    //identify machines, but the ip address can still be used.
   }
  Serial.println();
  //*/
  return(maskcmp(clientIp,ip,subnet_mask));
}

boolean ipEqual(uint8_t * addr1, uint8_t * addr2){
  return (memcmp(addr1,addr2,4)==0);
} 

// format an IP address. (I stole this)
const char* ip_to_str(const uint8_t* ipAddr)
{
  strcpy_P(Dbuf2,PSTR("%d.%d.%d.%d\0"));
  sprintf(Dbuf, Dbuf2, ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
  return (Dbuf);
}
