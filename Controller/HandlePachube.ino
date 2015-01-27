
void sendPachubeData(){
  char dataBuf[100];
  
  if(pachube.connected()) // already trying to get data, just leave
    return;
  if(round(realPower) == 0.0)  // don't send invalid data
    return;
  tNow = now();
  strcpy_P(Dbuf2,PSTR("Data at %02d:%02d:%02d: "));  // Debugging
  sprintf(Dbuf,Dbuf2,hour(tNow),minute(tNow),second(tNow));
//  Serial.print(Dbuf);
  // construct the data buffer so we know how long it is
  int poolMotorState = 0;
  if(strcmp(poolData.motorState,"High") == 0)
     poolMotorState = 2;
  else if(strcmp(poolData.motorState,"Low") == 0)
    poolMotorState = 1;
  strcpy_P(Dbuf2, PSTR("%d,%d,0.%d,%d.%02d,%d.%02d,%d.%02d,%d,%d,%d,%d"));
  sprintf(dataBuf,Dbuf2,
    (int)round(realPower),
    (int)round(apparentPower),
    (int)(powerFactor*100),
    (int)rmsCurrent,
    (int)(((rmsCurrent+0.005) - (int)rmsCurrent) * 100),
    (int)rmsVoltage,
    (int)(((rmsVoltage+0.005) - (int)rmsVoltage) * 100),
    (int)(frequency), 
    (int)(((frequency+0.005) - (int)frequency) * 100),
    (ThermoData[0].currentTemp + ThermoData[1].currentTemp)/2,
    (int)round(outsideSensor.temp),
    poolMotorState, 
    // if the pool is off, report 10, if the number is out of whack, limit it.
    poolMotorState == 0 ? 10 : (poolData.poolTemp <= 120 ? poolData.poolTemp : 120) );
    //Serial.println(dataBuf);
    //return;
  doggieTickle(); // allow pachube the entire watchdog period
  strcpy_P(Dbuf,PSTR("Pachube Connecting...")); 
  Serial.print(Dbuf);
  if(pachube.connect("www.pachube.com",80)){ // set a limit on how long the connect will wait
    strcpy_P(Dbuf,PSTR("OK..."));
    Serial.print(Dbuf);
    tNow = now();
    strcpy_P(Dbuf,PSTR("PUT /api/9511.csv HTTP/1.1\n"));
    pachube.write(Dbuf);
    strcpy_P(Dbuf,PSTR("Host: www.pachube.com\n"));
    pachube.write(Dbuf);
    strcpy_P(Dbuf,PSTR("X-PachubeApiKey: e24b69b27040f1ac44cb8d7a284e1f0c0c298f02c7e2cf18016d4c9a7f76fa42\n"));
    pachube.write(Dbuf);
    strcpy_P(Dbuf2,PSTR("Content-Length: %d\n"));
    sprintf(Dbuf,Dbuf2,strlen(dataBuf));
    pachube.write(Dbuf);
    strcpy_P(Dbuf,PSTR("Content-Type: text/csv\n"));
    pachube.write(Dbuf);
    strcpy_P(Dbuf,PSTR("Connection: close\n\n"));  // has an extra nl to end the transaction
    pachube.write(Dbuf);
    pachube.write(dataBuf);
    pachube.write("\n");
    //pachube.stop();    // not closing it here allows checking in loop()
    pachubeUpdateTime = now();
  }
  else {
    strcpy_P(Dbuf,PSTR("failed..."));
    Serial.print(Dbuf);
    pachube.stop();
    while(pachube.status() != 0){
      delay(5);
    }
  }
  strcpy_P(Dbuf,PSTR("Returning")); 
  Serial.println(Dbuf);
}
