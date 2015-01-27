boolean tryGetPublicIp()
{
  int waitcnt = 0;
  long timer = 0;
  
  boolean ipCheckConnected = false;
  while(1){
    if (!ipCheckConnected){
      Serial.print(F("ipCheck Connecting..."));
      if (ipCheck.connect("checkip.dyndns.com",80)) {
        ipCheckConnected = true;
        waitcnt++;
        Serial.println(F("OK"));
        strcpy_P(Dbuf,PSTR("GET / HTTP/1.1\n\n"));
//        Serial.println(Dbuf);  // for debug
        ipCheck.write(Dbuf);
        timer = millis(); // to time the response, just in case 
      }
      else{
        Serial.println(F("failed"));
        waitcnt++;
        timer = millis();  // if the connection failed I don't need to time it.
        ipCheck.flush();
        ipCheck.stop();
        while (ipCheck.status() != 0){
          delay(5);
        }
      }
    }
    if (ipCheck.available() > 0){
      if(ipfinder.find("Address:")){
        externalIp[0] = (byte)ipfinder.getValue();
        externalIp[1] = (byte)ipfinder.getValue();
        externalIp[2] = (byte)ipfinder.getValue();
        externalIp[3] = (byte)ipfinder.getValue();
        ipCheck.flush();
        ipCheck.stop();
        while (ipCheck.status() != 0){
          delay(5);
        }
        return(true);
      }
      Serial.println(F("Invalid Response from ipCheck()"));
      ipCheck.flush();
      ipCheck.stop();
      while (ipCheck.status() != 0){
        delay(5);
      }
      waitcnt++;
      ipCheckConnected = false;      
    }
    if (ipCheckConnected && (millis() - timer >6000)){
//      Serial.println(F("Timeout waiting"));
      ipCheck.flush();
      ipCheck.stop();
      while (ipCheck.status() != 0){
        delay(5);
      }
      ipCheckConnected = false;
      waitcnt++;
    }
    
    if (!ipCheckConnected){
      strcpy_P(Dbuf2,PSTR("ipCheck try # %d"));
      sprintf(Dbuf,Dbuf2,waitcnt);
      Serial.println(Dbuf);
      waitcnt++;
      delay (1000);
    }
    if (waitcnt > 9){
      ipCheck.flush();
      ipCheck.stop();
      while (ipCheck.status() != 0){
        delay(5);
      }
      return(false);
    }
  }
}

void getPublicIp(){
  if(tryGetPublicIp() == false){
    Serial.println(F("Public IP check failure, rebooting"));
    resetMe("getPublicIp");  //call reset if you can't get the comm to work
  }
  return;
}

boolean tryPublishIp()
{
  int waitcnt = 0;
  long timer = 0;
  boolean dyndnsConnected = false;

  while(1){
    if (!dyndnsConnected){
      Serial.print(F("dyndns Connecting..."));
      if (dyndns.connect("members.dyndns.org",80)) {
        dyndnsConnected = true;
        waitcnt++;
        Serial.println(F("OK"));
        strcpy_P(Dbuf,PSTR("GET /nic/update?hostname=thomeauto.is-a-geek.org"));
        dyndns.write(Dbuf);
        strcpy_P(Dbuf2,PSTR("&myip=%d.%d.%d.%d&wildcard=NOCHG"));
        sprintf(Dbuf,Dbuf2,externalIp[0],externalIp[1],externalIp[2],externalIp[3]);
        dyndns.write(Dbuf);
        strcpy_P(Dbuf,PSTR("&mx=NOCHG&backmx=NOCHG HTTP/1.0\n"));
        dyndns.write(Dbuf);
        strcpy_P(Dbuf,PSTR("Host: members.dyndns.org\n"));
        dyndns.write(Dbuf);
 /*let me in */
        strcpy_P(Dbuf,PSTR("Authorization: Basic ZHJheXRob21wOmxldG1laW4=\n"));
        dyndns.write(Dbuf);
        strcpy_P(Dbuf,PSTR("User-Agent: Thompson Home Control - Arduino Controller"));
        dyndns.write(Dbuf);
        strcpy_P(Dbuf,PSTR(" - Version 5\n\n"));
        dyndns.write(Dbuf);
        timer = millis(); // to time the response, just in case 
      }
      else {
        Serial.print(F("failed"));
        waitcnt++;
        timer = millis();  // if the connection failed I don't need to time it.
        dyndns.flush();
        dyndns.stop();
        while (dyndns.status() != 0){
          delay(5);
        }
      }
    }
    if (dyndns.available() > 0){
      while(dyndns.available()){
        int c = dyndns.read();  //Sometime I may need to check this
//        Serial.print(c,BYTE);
      }
      dyndns.flush();
      dyndns.stop();
      while (dyndns.status() != 0){
        delay(5);
      }
      return(true);
    }
    if (dyndnsConnected && (millis() - timer >5000)){
      Serial.println(F("Timeout waiting"));
      dyndns.flush();
      dyndns.stop();
      while (dyndns.status() != 0){
        delay(5);
      }
      dyndnsConnected = false;
    }
    
    if (!dyndnsConnected){
      strcpy_P(Dbuf2,PSTR("dyndns try # %d"));
      sprintf(Dbuf,Dbuf2,waitcnt);
      Serial.println(Dbuf);
      delay (1000);
    }
    if (waitcnt > 9){
      dyndns.flush();
      dyndns.stop();
      while (dyndns.status() != 0){
        delay(5);
      }
      return(false);
    }
  }
}

void publishIp(){
  if(tryPublishIp() == false){
    Serial.println(F("Publish IP failure, rebooting"));
    resetMe("publishIp");  //call reset if you can't get the comm to work
  }
  return;
}

