XBeeAddress64 HouseController = XBeeAddress64(0x0013A200, 0x4055B0A6);
XBeeAddress64 Broadcast       = XBeeAddress64(0x00000000, 0x0000ffff);
XBeeAddress64 HouseController2 = XBeeAddress64(0x0013A200, 0x406f7f8c);

void sendStatusXbee(char *msg){
  byte checksum = 0;
  char xbeeOut[100];
  int i, length;
  
  digitalWrite(6,HIGH);               // indicator light on
  Alarm.timerOnce(1, indicatorOff);   // Turn the indicator led off in a second
  ZBTxRequest zbtx = ZBTxRequest(HouseController, (uint8_t *)msg, strlen(msg));
  xbee.send(zbtx);
  ZBTxRequest zbtx2 = ZBTxRequest(HouseController2, (uint8_t *)msg, strlen(msg));
  xbee.send(zbtx2);
}

void indicatorOff(){
  digitalWrite(6,LOW);
}
