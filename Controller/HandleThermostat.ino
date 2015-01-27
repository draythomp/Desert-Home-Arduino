//
boolean Thermoconnected = false;

boolean tryGetThermostat(int idx,char* command, boolean respReq){
  char* ptr;
  char* data;
  int cnt, len;
  int waitcnt = 0;
  long timer = 0;
  EthernetClient thermoClient;
  
  while(1){
    if (!Thermoconnected){
      strcpy_P(Dbuf2,PSTR("%s Thermostat Connecting..."));
      sprintf(Dbuf,Dbuf2, (idx==0) ? "North" : "South");
//      Serial.print(Dbuf);
//      Serial.println(idx == 0 ? NThermoAddress : SThermoAddress );

      if (thermoClient.connect(idx == 0 ? NThermoAddress : SThermoAddress, 80)) {
        Thermoconnected = true;
        waitcnt++;
        strcpy_P(Dbuf,PSTR("OK..."));
//        Serial.print(Dbuf);
        strcpy_P(Dbuf2,PSTR("GET /%s HTTP/1.0\n"));
        sprintf(Dbuf,Dbuf2,command);
//        Serial.println(Dbuf);  // for debug
        thermoClient.write(Dbuf);
        timer = millis(); // to time the response, just in case 
      }
      else{
        strcpy_P(Dbuf,PSTR("failed..."));
        Serial.print(Dbuf);
        waitcnt++;
        timer = millis();  // if the connection failed I don't need to time it.
        thermoClient.flush();
        thermoClient.stop();
        while (thermoClient.status() != 0){
          delay(5);
        }
      }
    }
    if (thermoClient.available() > 0){
      ptr = Dbuf;
      while (thermoClient.available()) {
        *ptr++ = thermoClient.read();
      }
      thermoClient.flush(); //suck out any extra chars  
      thermoClient.stop();
      while (thermoClient.status() != 0){
        delay(5);
      }
      Thermoconnected = false;
      
      *ptr='\0';
      if(!respReq){ //response not required
        return(true);
      }
//       Serial.println(Dbuf+44); //debug only
      // finally got some data
      ThermoData[idx].reportTime = now();  // record the time this happened.
      ThermoData[idx].currentTemp = atof(strtok_r(Dbuf+44, ",", &ptr));   // first item is current temp
      data = strtok_r(0, ",", &ptr);                  // first find the token
      len = strcspn(data, ",");                       // then the length
      memset(ThermoData[idx].state,0,sizeof(ThermoData[idx].state)); //make sure it's empty
      strncpy(ThermoData[idx].state, data, len);          // copy out what it's doing currently
      ThermoData[idx].tempSetting = atof(strtok_r(ptr, ",", &ptr));   // temp setting
      data = strtok_r(0, ",", &ptr);  
      len = strcspn(data, ","); 
      memset(ThermoData[idx].mode,0,sizeof(ThermoData[idx].mode));
      strncpy(ThermoData[idx].mode, data, len);           // Mode setting (cool, heat, off)
      data = strtok_r(0, ",", &ptr);  
      len = strcspn(data, ","); 
      memset(ThermoData[idx].fan,0,sizeof(ThermoData[idx].fan));
      strncpy(ThermoData[idx].fan, data, len);            // Fan setting (Auto, On, Recirc - no off)
      data = strtok_r(0, ",", &ptr);  
      len = strcspn(data, ",\n\r"); 
      memset(ThermoData[idx].peak,0,sizeof(ThermoData[idx].peak));
      strncpy(ThermoData[idx].peak, data, len);           // In peak period ?
      strcpy_P(Dbuf2,PSTR("Temp is %d, Heat pump is %s, \nTemp Setting is %d, Mode %s, Fan %s, %s Period"));
/*      sprintf(Dbuf,Dbuf2, ThermoData[idx].currentTemp,
        ThermoData[idx].state,
        ThermoData[idx].tempSetting,
        ThermoData[idx].mode,
        ThermoData[idx].fan,
        ThermoData[idx].peak);
      Serial.println(Dbuf); */
      // status LEDs are updated here
      if (strcmp_P(ThermoData[idx].state,PSTR("Cooling")) == 0){
        if (idx == 0){
          digitalWrite(nRecircLed, HIGH);
          digitalWrite(nCoolLed, LOW);
          digitalWrite(nHeatLed, HIGH);
        }
        else {
          digitalWrite(sRecircLed, HIGH);
          digitalWrite(sCoolLed, LOW);
          digitalWrite(sHeatLed, HIGH);
        }
      }
      else if (strcmp_P(ThermoData[idx].state,PSTR("Heating")) == 0){
        if (idx == 0){
          digitalWrite(nRecircLed, HIGH);
          digitalWrite(nCoolLed, HIGH);
          digitalWrite(nHeatLed, LOW);
        }
        else {
          digitalWrite(sRecircLed, HIGH);
          digitalWrite(sCoolLed, HIGH);
          digitalWrite(sHeatLed, LOW);
        }
      }
      else if (strcmp_P(ThermoData[idx].state,PSTR("Recirc")) == 0){
        if (idx == 0){
          digitalWrite(nRecircLed, LOW);
          digitalWrite(nCoolLed, HIGH);
          digitalWrite(nHeatLed, HIGH);
        }
        else {
          digitalWrite(sRecircLed, LOW);
          digitalWrite(sCoolLed, HIGH);
          digitalWrite(sHeatLed, HIGH);
        }
      }
      else { // idle state is all LEDs off
        if (idx == 0){
          digitalWrite(nRecircLed, HIGH);
          digitalWrite(nCoolLed, HIGH);
          digitalWrite(nHeatLed, HIGH);
        }
        else {
          digitalWrite(sRecircLed, HIGH);
          digitalWrite(sCoolLed, HIGH);
          digitalWrite(sHeatLed, HIGH);
        }
      }
      return(true);
    }
    if (millis() - timer >5000){
      strcpy_P(Dbuf,PSTR("Timeout waiting"));
      Serial.println(Dbuf);
      thermoClient.flush();
      thermoClient.stop();
      while (thermoClient.status() != 0){
        delay(5);
      }
      Thermoconnected = false;
    }
    
    if (!Thermoconnected){
      strcpy_P(Dbuf2,PSTR("Thermo try # %d"));
      sprintf(Dbuf,Dbuf2,waitcnt);
      Serial.println(Dbuf);
      delay (1000);
    }
    if (waitcnt > 9){
      thermoClient.flush();
      thermoClient.stop();
      while (thermoClient.status() != 0){
        delay(5);
      }
      return(false);
    }
  }
}
void getThermostat(int idx, char* command, boolean respReq ){
  if(tryGetThermostat(idx, command, respReq) == false){
    strcpy_P(Dbuf,PSTR("Thermo comm failure, rebooting"));
    Serial.println(Dbuf);
    resetMe("getThermostat");  //call reset if you can't get the comm to work
  }
  strcpy_P(Dbuf,PSTR("Returning"));
//  Serial.println(Dbuf);
  return;
}

