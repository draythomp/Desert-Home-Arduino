
void sendEmoncmsData(){
  char dataBuf[100];
  
  if(emoncms.connected()) // already trying to get data, just leave
    return;

  if(round(realPower) == 0.0)  // don't send invalid data
    return;
  tNow = now();
  strcpy_P(Dbuf2,PSTR("Data at %02d:%02d:%02d: "));  // Debugging
  sprintf(Dbuf,Dbuf2,hour(tNow),minute(tNow),second(tNow));
//  Serial.print(Dbuf);
  // construct the data buffer so we know how long it is
  strcpy_P(Dbuf2, PSTR("{RealPower:%d,PowerFactor:0.%d,PowerVoltage:%d.%02d,PowerFrequency:%d.%02d,InsideTemp:%d,OutsideTemp:%d}"));
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
  doggieTickle(); // allow emoncms the entire watchdog period
  strcpy_P(Dbuf,PSTR("emoncms Connecting...")); 
  Serial.print(Dbuf);
  if(emoncms.connect("emoncms.org",80)){
    strcpy_P(Dbuf,PSTR("OK..."));
    Serial.print(Dbuf);
    tNow = now();
    strcpy_P(Dbuf,PSTR("GET http://emoncms.org/input/post?apikey=99b59266894a9b7bcae9f57e73a64f81&json="));
    emoncms.write(Dbuf);
    emoncms.write(dataBuf);
    emoncms.write("\n");
    //emoncms.stop();    // not closing it here allows checking in loop()
    emoncmsUpdateTime = now();
  }
  else {
    strcpy_P(Dbuf,PSTR("failed..."));
    Serial.print(Dbuf);
    emoncms.stop();
    while(emoncms.status() != 0){
      delay(5);
    }
  }
  strcpy_P(Dbuf,PSTR("Returning")); 
  Serial.println(Dbuf);
}
