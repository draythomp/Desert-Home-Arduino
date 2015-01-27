
void sendThingspeakData(){
  char dataBuf[100];
  
  if(thingspeak.connected()) // already trying to get data, just leave
    return;

  if(round(realPower) == 0.0)  // don't send invalid data
    return;
  tNow = now();
  strcpy_P(Dbuf2,PSTR("Data at %02d:%02d:%02d: "));  // Debugging
  sprintf(Dbuf,Dbuf2,hour(tNow),minute(tNow),second(tNow));
//  Serial.print(Dbuf);
  // construct the data buffer so we know how long it is
  // thingspeak uses channels and fields.  Each channel can have 8 fields named field1, field2, etc.  I have
  // look them up to see which is which on the site itself.  This may take a little more learning, but for
  // now I just want to get some data up there to see how it works.
  strcpy_P(Dbuf2, PSTR("field1=%d&field2=0.%d&field3=%d.%02d&field4=%d.%02d&field5=%d&field6=%d"));
  sprintf(dataBuf,Dbuf2,
    (int)round(realPower),
    (int)(powerFactor*100),
    (int)rmsVoltage,
    (int)(((rmsVoltage+0.005) - (int)rmsVoltage) * 100),
    (int)(frequency), 
    (int)(((frequency+0.005) - (int)frequency) * 100),
    (ThermoData[0].currentTemp + ThermoData[1].currentTemp)/2,
    (int)round(outsideSensor.temp));
//    Serial.println(dataBuf);
//    return;
  doggieTickle(); // allow thingspeak the entire watchdog period
  strcpy_P(Dbuf,PSTR("thingspeak Connecting...")); 
  Serial.print(Dbuf);
  if(thingspeak.connect("api.thingspeak.com",80)){
    strcpy_P(Dbuf,PSTR("OK..."));
    Serial.print(Dbuf);
    tNow = now();
    strcpy_P(Dbuf,PSTR("POST /update HTTP/1.1\n"));
    thingspeak.write(Dbuf);
    strcpy_P(Dbuf,PSTR("Host: api.thingspeak.com\n"));
    thingspeak.write(Dbuf);
    strcpy_P(Dbuf,PSTR("Connection: close\n"));
    thingspeak.write(Dbuf);
    strcpy_P(Dbuf,PSTR("X-THINGSPEAKAPIKEY: K1CEBPI4OJ5UCL8S\n"));
    thingspeak.write(Dbuf);
    strcpy_P(Dbuf,PSTR("Content-Type: application/x-www-form-urlencoded\n"));
    thingspeak.write(Dbuf);
    strcpy_P(Dbuf2,PSTR("Content-Length: %d\n\n"));
    sprintf(Dbuf,Dbuf2,strlen(dataBuf));
    thingspeak.write(Dbuf);
    thingspeak.write(dataBuf);
    //thingspeak.stop();    // not closing it here allows checking in loop()
    thingspeakUpdateTime = now();
  }
  else {
    strcpy_P(Dbuf,PSTR("failed..."));
    Serial.print(Dbuf);
    thingspeak.stop();
    while(thingspeak.status() != 0){
      delay(5);
    }
  }
  strcpy_P(Dbuf,PSTR("Returning")); 
  Serial.println(Dbuf);
} 
