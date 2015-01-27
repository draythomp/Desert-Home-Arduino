void acidPumpOn(){
  if (digitalRead(acidPump) == RELAYCLOSED){
    acidPumpReport();
    return;  // it's already on, just return
  }
  digitalWrite(acidPump, RELAYCLOSED); // turn on the pump
  pumping = true;
  if(Alarm.isAllocated(offTimer)){ //if there is already a timer running, stop it.
    Alarm.free(offTimer);
    offTimer = dtNBR_ALARMS;       // to indicate there is no timer active
  }
  offTimer = Alarm.timerOnce(PUMPTIME, acidPumpOff);  // automatically turn it off in 10 min
  Serial.println("Acid Pump On");
  acidPumpReport();
}

void acidPumpOff(){
  if (Alarm.isAllocated(offTimer)){ //If there is an off timer running, stop it.
    Alarm.free(offTimer);
    offTimer = dtNBR_ALARMS;
  }
  // doesn't hurt anything to turn it off multiple times.
  digitalWrite(acidPump, RELAYOPEN); // turn the pump off
  pumping = false;
  Serial.println("Acid Pump Off");
  acidPumpReport();
}

