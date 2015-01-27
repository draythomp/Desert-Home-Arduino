// All the timers and some of the supporting routines are here
// if the number of timers and alarms exceeds 6 update constant dtNBR_ALARMS
// to match

void initTimers(){
  // I want the various timers to work on the minute boundary
  Serial.print(F("Setting Timers Up..."));
  while(second() != 0){
    delay(500);
    doggieTickle(); // so the watchdog doesn't time out on me
  }
  // I have to stagger the timers for the ethernet, otherwise, they all hit
  // at the same time and I run out of connections (maximum of 4)
  Alarm.timerOnce(5, updateThermostats);    // Kick start the Thermostats, they'll be set to
                                            // every 30 seconds by updateThermostats.
  Alarm.timerOnce(10, updatePachube);     // update data servers every minute
  Alarm.timerOnce(15, updateEmoncms);
  Alarm.timerOnce(20, updateThingspeak);
  Alarm.timerRepeat(10*60,checkPublicIp);   // every 10 min check public IP
  
  Alarm.alarmRepeat(11,55,0,heatpumpWeekdayFanAuto);  // every weekday set the fans to auto to prevent recirc
                                                      // the thermostats will turn off the compressor automatically
  Alarm.alarmRepeat(19,1,0,heatpumpFanRecirc);  //It's nice to have the air moving around in the evening.
  //Alarm.alarmRepeat(19, 1, 0,winterNight);  // Set thermostats for a winter night
  //Alarm.alarmRepeat(11, 0, 0,summerNight);  // set thermostats for summer night
  
  Alarm.alarmRepeat(22,0,0,poolMotorOff);      // Pool motor off at 10PM in case I forgot
  Alarm.alarmRepeat(8,15,0,acidPumpOddDay);    // will run the acid pump on odd number days

  Alarm.alarmOnce(dowTuesday,7,30,0,testRoutine);  //next Tuesday at 0730 this will just test the timers
  
  Alarm.timerOnce(5,rotatingDisplay);  // this kicks off the rotating display
  Alarm.alarmRepeat(23,59,0,autoReset);  // periodic reset to keep things cleaned up
                                       // I use a lot of libraries and sometimes they have bugs
                                       // as well as hang ups from various hardware devices
  Serial.println(F("done"));
  Alarm.delay(0);
}

void testRoutine(){
  strcpy_P(Dbuf2,PSTR("Alarm or timer fired at %02d/%02d/%02d %02d:%02d:%02d"));
  sprintf(Dbuf,Dbuf2,month(),day(),year(),hour(),minute(),second());
  Serial.println(Dbuf);
}

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void autoReset(){
  Serial.println("Periodic Reset - Normal Operation");
  resetMe("autoReset");
}

void resetMe(char *location){  // for periodic resets to be sure nothing clogs it up
  Serial.print("intentional reset from ");
  Serial.println(location);
  showMem();
  delay(10);  // this is to allow the message time to get out.
  resetFunc();
}

void updatePachube(){
  /*
  Since the data all comes in asyncronously these values
  can be invalid when the device starts up.  Check to be 
  sure there is something there before continuing
  */
  Alarm.timerOnce(60, updatePachube); // do it again in 60 seconds.
  if(pachube.connected()) // already trying to do this, just leave
    return;
  if(round(realPower) == 0.0)  // don't send invalid data
    return;
  if(round(outsideSensor.temp) == 0.0)
    return;
  if(ThermoData[0].currentTemp == 0)
    return;
  if(ThermoData[1].currentTemp == 0)
    return;
  sendPachubeData();
}


void updateEmoncms(){
  /*
  Since the data all comes in asyncronously these values
  can be invalid when the device starts up.  Check to be 
  sure there is something there before continuing
  */
  Alarm.timerOnce(60, updateEmoncms);
  if(emoncms.connected()) // already trying to do this, just leave
    return;
  if(round(realPower) == 0.0)  // don't send invalid data
    return;
  if(round(outsideSensor.temp) == 0.0)
    return;
  if(ThermoData[0].currentTemp == 0)
    return;
  if(ThermoData[1].currentTemp == 0)
    return;
  sendEmoncmsData();
}

void updateThingspeak(){
  /*
  Since the data all comes in asyncronously these values
  can be invalid when the device starts up.  Check to be 
  sure there is something there before continuing
  */
  Alarm.timerOnce(60, updateThingspeak);  // do it again
  if(thingspeak.connected()) // already trying to do this, just leave
    return;
  if(round(realPower) == 0.0)  // don't send invalid data
    return;
  if(round(outsideSensor.temp) == 0.0)
    return;
  if(ThermoData[0].currentTemp == 0)
    return;
  if(ThermoData[1].currentTemp == 0)
    return;
  sendThingspeakData();
}

// Theromostat handling routines

void updateThermostats(){  // this will interogate the thermostats to make sure data is current
  Alarm.timerOnce(30, updateThermostats);  // check again in 30 seconds.
//  Serial.print(F("update thermostats at "));
//  Serial.print(minute());
//  Serial.print(F(":"));
//  Serial.println(second());
  getThermostat(0, "status", true);
  getThermostat(1, "status", true);
}
// save and restore for the thermostat data.  This way it can be returned
// to normal after an automatic change
//
struct thermoData savedThermoData[2];  // saved thermostat data
void saveThermoData(){  // this will save the settings so they can be restored
  updateThermostats(); // make sure we have the latest settings
  for (int i=0; i<2; i++){
    savedThermoData[0] = ThermoData[0];
    savedThermoData[1] = ThermoData[1];
    sprintf(Dbuf,"I saved for thermostat %i, temp=%d, mode=%s, fan=%s", i,
      savedThermoData[i].tempSetting,savedThermoData[i].mode,savedThermoData[i].fan);
//    Serial.println(Dbuf);
  }
}

// and this will restore the settings saved above
void restoreThermoData(){  
  char command[10];
  
  for(int i = 0; i<2; i++){
    sprintf(Dbuf,"temp=%d",savedThermoData[i].tempSetting);
//    Serial.println(Dbuf);
    getThermostat(i,Dbuf,false); // set the temperature to what it was when saved
  
    if(strcmp(savedThermoData[i].fan, "Auto") == 0)
      strcpy(command,"auto");
    else if(strcmp(savedThermoData[i].fan, "On") == 0)
      strcpy(command,"on");
    else if(strcmp(savedThermoData[i].fan, "Recirc") == 0)
      strcpy(command,"recirc");
    sprintf(Dbuf,"fan=%s",command);
//    Serial.println(Dbuf);
    getThermostat(i,Dbuf,false);  // set the fan to what is was before
    
    if(strcmp(savedThermoData[i].mode, "Cooling")== 0)
      strcpy(command,"cool");
    else if(strcmp(savedThermoData[i].mode, "Heating") == 0)
      strcpy(command,"heat");
    else if(strcmp(savedThermoData[i].mode, "Off") == 0)
      strcpy(command,"off");
    sprintf(Dbuf,"%s",command);
//    Serial.println(Dbuf);
    getThermostat(i,Dbuf,false);  // set the mode to what it was before
  }
  updateThermostats(); //update the local status for them
}
/*
  Routines to control heatpump cooling functions
*/
void heatpumpFanRecirc(){
  getThermostat(0,"fan=recirc", false); //Set the North one to auto
  getThermostat(1,"fan=recirc", false); //and the South one too
}
void heatpumpWeekdayFanAuto(){
    if(weekday() == 1 || weekday() == 7) // Saturday and Sunday are not peak days, don't bother
    return;
    heatpumpFanAuto();
}

void heatpumpFanAuto(){
  getThermostat(0,"fan=auto", false); //Set the North one to auto
  getThermostat(1,"fan=auto", false); //and the South one too
}

void heatpumpTo75(){
  if(weekday() == 1 || weekday() == 7) // Saturday and Sunday are not peak days, don't bother
    return;
  getThermostat(0,"temp=75", false); //Set the North one
  getThermostat(1,"temp=75", false); //and the South one too
}
void heatpumpTo98(){
  getThermostat(0,"temp=98", false); //Set the North one
  getThermostat(1,"temp=98", false); //and the South one too
  heatpumpFanAuto();
}

void summerNight(){
  strcpy(Dbuf,PSTR("Going into Summer Night Mode\n\n"));
  Serial.print(Dbuf);
  // not busy, so mess with them
  saveThermoData();  // save what the settings were in case we want to restore them.
  //set the temp on both of them.
  getThermostat(0,"temp=78", false); //Set the North one
  getThermostat(1,"temp=79", false); //and the South one too
  //now, the fans should be recirc to keep the temp even
  heatpumpFanRecirc();
  getThermostat(0,"cool",false);  // and make it cool.
  getThermostat(1,"cool",false);
}

void winterNight(){
  strcpy(Dbuf,PSTR("Going into Winter Night Mode\n\n"));
  Serial.print(Dbuf);
  // not busy, so mess with them
  saveThermoData();  // save what the settings were in case we want to restore them.
  //set the temp on both of them.
  getThermostat(0,"temp=73", false); //Set the North one
  getThermostat(1,"temp=72", false); //and the South one too
  //now, the fans should be recirc to keep the temp even
  heatpumpFanRecirc();
  getThermostat(0,"heat",false);  // and turn on the heat
  getThermostat(1,"heat",false);
}

void peakOff(){
  saveThermoData();  // save what the settings were in case we want to restore them.
  //set the temp on both of them.
  getThermostat(0,"peakoff", false); //Set the North one
  getThermostat(1,"peakoff", false); //and the South one too
}

void peakOn(){
  saveThermoData();  // save what the settings were in case we want to restore them.
  //set the temp on both of them.
  getThermostat(0,"peakon", false); //Set the North one
  getThermostat(1,"peakon", false); //and the South one too
}

void airoff(){
  strcpy(Dbuf,PSTR("Shutting A/C off\n\n"));
  Serial.print(Dbuf);
  // not busy, so mess with them
  saveThermoData();  // save what the settings were in case we want to restore them.
  //now, the fans should be set to auto
  heatpumpFanAuto();
  getThermostat(0,"off",false);  // turn the compressors off
  getThermostat(1,"off",false);
}

// End of Thermostat Routines

void checkPublicIp(){
  //Check the externally visible IP and update if necessary.
  strcpy_P(Dbuf,PSTR("Public IP address check..."));
  Serial.print(Dbuf);
  getPublicIp();
  Serial.print(ip_to_str(externalIp));
  if(!ipEqual(externalIp, eepromData.lastIp)){
    publishIp();
    memcpy(eepromData.lastIp,externalIp,4);
    eepromAdjust();
    strcpy_P(Dbuf,PSTR("...updated"));
    Serial.println(Dbuf);
    return;
  }
  strcpy_P(Dbuf,PSTR("...no change"));
  Serial.println(Dbuf);
 }

void timeRxLedOff(){
  digitalWrite(timeRxLed,HIGH);
}
void tempRxLedOff(){
  digitalWrite(tempRxLed,HIGH);
}
void powerRxLedOff(){
  digitalWrite(powerRxLed,HIGH);
}
void etherRxLedOff(){
  digitalWrite(etherRxLed,HIGH);
}
void extEtherRxLedOff(){
  digitalWrite(extEtherRxLed,HIGH);
}
void statusRxLedOff(){
  digitalWrite(statusRxLed,HIGH);
}
void poolRxLedOff(){
  digitalWrite(poolRxLed,HIGH);
}
void acidPumpOddDay(){  //run the acid pump on odd days
  if(dayOfYear() % 2 == 0)
    return;
  acidPumpOn();  
}

// returns the day of year 1-365 or 366 so Feb 29th is the 60th day.
// this is used for things like even days, odd days, every third day
// that kind of thing since a week has 7 days and a month varies.  
// sprinklers and such may need this kind of calculation
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



