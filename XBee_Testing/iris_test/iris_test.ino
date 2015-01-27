/**
This is an examination of Zigbee device communication using an XBee
and an Iris Smart Switch from Lowe's
*/
 
#include <XBee.h>
#include <SoftwareSerial.h>
#include <Time.h>
#include <TimeAlarms.h>

XBee xbee = XBee();
XBeeResponse response = XBeeResponse();
// create reusable response objects for responses we expect to handle 
ZBExpRxResponse rx = ZBExpRxResponse();

// Define NewSoftSerial TX/RX pins
// Connect Arduino pin 2 to Tx and 3 to Rx of the XBee
// I know this sounds backwards, but remember that output
// from the Arduino is input to the Xbee
#define ssRX 2
#define ssTX 3
SoftwareSerial nss(ssRX, ssTX);

XBeeAddress64 switchLongAddress;
uint16_t switchShortAddress;
uint16_t payload[50];
uint16_t myFrameId=1;

void setup() {  
  // start serial
  Serial.begin(9600);
  // and the software serial port
  nss.begin(9600);
  // now that they are started, hook the XBee into 
  // Software Serial
  xbee.setSerial(nss);
  // I think this is the only line actually left over
  // from Andrew's original example
  setTime(0,0,0,1,1,14);  // just so alarms work well, I don't really need the time.
  Serial.println("started");
}

void loop() {
    // doing the read without a timer makes it non-blocking, so
    // you can do other stuff in loop() as well.  Things like
    // looking at the console for something to turn the switch on
    // or off (see waaay down below)
    xbee.readPacket();
    // so the read above will set the available up to 
    // work when you check it.
    if (xbee.getResponse().isAvailable()) {
      // got something
//      Serial.println();
//      Serial.print("Frame Type is ");
      // Andrew called the XBee frame type ApiId, it's the first byte
      // of the frame specific data in the packet.
//      Serial.println(xbee.getResponse().getApiId(), HEX);
      //
      // All ZigBee device interaction is handled by the two XBee message type
      // ZB_EXPLICIT_RX_RESPONSE (ZigBee Explicit Rx Indicator Type 91)
      // ZB_EXPLICIT_TX_REQUEST (Explicit Addressing ZigBee Command Frame Type 11)
      // This test code only uses these and the Transmit Status message
      //
      if (xbee.getResponse().getApiId() == ZB_EXPLICIT_RX_RESPONSE) {
        // now that you know it's a Zigbee receive packet
        // fill in the values
        xbee.getResponse().getZBExpRxResponse(rx);
        
        // get the 64 bit address out of the incoming packet so you know 
        // which device it came from
//        Serial.print("Got a Zigbee explicit packet from: ");
        XBeeAddress64 senderLongAddress = rx.getRemoteAddress64();
//        print32Bits(senderLongAddress.getMsb());
//        Serial.print(" ");
//        print32Bits(senderLongAddress.getLsb());
        
        // this is how to get the sender's
        // 16 bit address and show it
        uint16_t senderShortAddress = rx.getRemoteAddress16();
//        Serial.print(" (");
//        print16Bits(senderShortAddress);
//        Serial.println(")");
        
        // for right now, since I'm only working with one switch
        // save the addresses globally for the entire test module
        switchLongAddress = rx.getRemoteAddress64();
        switchShortAddress = rx.getRemoteAddress16();

        //Serial.print("checksum is 0x");
        //Serial.println(rx.getChecksum(), HEX);
        
        // this is the frame length
        //Serial.print("frame data length is ");
        int frameDataLength = rx.getFrameDataLength();
        //Serial.println(frameDataLength, DEC);
        
        uint8_t* frameData = rx.getFrameData();
        // display everything after first 10 bytes
        // this is the Zigbee data after the XBee supplied addresses
//        Serial.println("Zigbee Specific Data from Device: ");
//        for (int i = 10; i < frameDataLength; i++) {
//          print8Bits(frameData[i]);
//          Serial.print(" ");
//        }
//        Serial.println();
        // get the source endpoint
//        Serial.print("Source Endpoint: ");
//        print8Bits(rx.getSrcEndpoint());
//        Serial.println();
        // byte 1 is the destination endpoint
//        Serial.print("Destination Endpoint: ");
//        print8Bits(rx.getDestEndpoint());
//        Serial.println();
        // bytes 2 and 3 are the cluster id
        // a cluster id of 0x13 is the device announce message
//        Serial.print("Cluster ID: ");
        uint16_t clusterId = (rx.getClusterId());
        print16Bits(clusterId);
        Serial.print(": ");
        // bytes 4 and 5 are the profile id
//        Serial.print("Profile ID: ");
//        print16Bits(rx.getProfileId());
//        Serial.println();
//        // byte 6 is the receive options
//        Serial.print("Receive Options: ");
//        print8Bits(rx.getRxOptions());
//        Serial.println();
//        Serial.print("Length of RF Data: ");
//        Serial.print(rx.getRFDataLength());
//        Serial.println();
//        Serial.print("RF Data Received: ");
//        for(int i=0; i < rx.getRFDataLength(); i++){
//            print8Bits(rx.getRFData()[i]);
//            Serial.print(' ');
//        }
//        Serial.println();
        //
        // I have the message and it's from a ZigBee device
        // so I have to deal with things like cluster ID, Profile ID
        // and the other strangely named fields that these devices use
        // for information and control
        //
        if (clusterId == 0x13){
          Serial.println("*** Device Announce Message");
          // In the announce message:
          // the next bytes are a 16 bit address and a 64 bit address (10) bytes
          // that are sent 'little endian' which means backwards such
          // that the most significant byte is last.
          // then the capabilities byte of the actual device, but
          // we don't need some of them because the XBee does most of the work 
          // for us.
          //
          // so save the long and short addresses
          switchLongAddress = rx.getRemoteAddress64();
          switchShortAddress = rx.getRemoteAddress16();
          // the data carried by the Device Announce Zigbee messaage is 18 bytes over
          // 2 for src & dest endpoints, 4 for cluster and profile ID, 
          // receive options 1, sequence number 1, short address 2, 
          // long address 8 ... after that is the data specific to 
          // this Zigbee message
//          Serial.print("Sequence Number: ");
//          print8Bits(rx.getRFData()[0]);
//          Serial.println();
//          Serial.print("Device Capabilities: ");
//          print8Bits(rx.getRFData()[11]);
//          Serial.println();
        }
        if (clusterId == 0x8005){ // Active endpoint response
          Serial.println("*** Active Endpoint Response");
          // You should get a transmit responnse packet back from the
          // XBee first, this will tell you the other end received 
          // something.
          // Then, an Active Endpoint Response from the end device
          // which will be Source Endpoint 0, Dest Endpoint 0,
          // Cluster ID 8005, Profile 0
          // it will have a payload, but the format returned by the 
          // Iris switch doesn't match the specifications.
          //
          // Also, I tried responding to this message directly after
          // its receipt, but that didn't work.  When I moved the 
          // response to follow the receipt of the Match Descriptor
          // Request, it started working.  So look below for where I 
          // send the response
        }
        if (clusterId == 0x0006){ // Match descriptor request
          Serial.println("*** Match Descriptor Request");
          // This is where I send the Active Endpoint Request 
          // which is endpoint 0x00, profile (0), cluster 0x0005
          uint8_t payload1[] = {0,0};
          ZBExpCommand tx = ZBExpCommand(switchLongAddress,
            switchShortAddress,
            0,    //src endpoint
            0,    //dest endpoint
            0x0005,    //cluster ID
            0x0000, //profile ID
            0,    //broadcast radius
            0x00,    //option
            payload1, //payload
            sizeof(payload1),    //payload length
            myFrameId++);   // frame ID
          xbee.send(tx);
//          Serial.println();
          //sendSwitch(0, 0, 0x0005, 0x0000, 0, 0, 0);

          Serial.print("sent active endpoint request ");
          //
          // So, send the next message, Match Descriptor Response,
          // cluster ID 0x8006, profile 0x0000, src and dest endpoints
          // 0x0000; there's also a payload byte
          //
          // {00.02} gave clicks
          uint8_t payload2[] = {0x00,0x00,0x00,0x00,0x01,02};
          tx = ZBExpCommand(switchLongAddress,
            switchShortAddress,
            0,    //src endpoint
            0,    //dest endpoint
            0x8006,    //cluster ID
            0x0000, //profile ID
            0,    //broadcast radius
            0x00,    //option
            payload2, //payload
            sizeof(payload2),    //payload length
            myFrameId++);   // frame ID
          xbee.send(tx);
//          Serial.println();
          Serial.print("sent Match Descriptor Response frame ID: ");
          Serial.println(myFrameId-1);
            
          //
          // Odd hardware message #1.  The next two messages are related 
          // to control of the hardware.  The Iris device won't stay joined with
          // the coordinator without both of these messages
          //
          uint8_t payload3[] = {0x11,0x01,0x01};
          tx = ZBExpCommand(switchLongAddress,
            switchShortAddress,
            0,    //src endpoint
            2,    //dest endpoint
            0x00f6,    //cluster ID
            0xc216, //profile ID
            0,    //broadcast radius
            0x00,    //option
            payload3, //payload
            sizeof(payload3),    //payload length
            myFrameId++);   // frame ID
            xbee.send(tx);
//            Serial.println();
            Serial.print("sent funny hardware message #1 frame ID: ");
            Serial.println(myFrameId-1);
            //
            // Odd hardware message #2
            //
            uint8_t payload4[] = {0x19,0x01,0xfa,0x00,0x01};
            tx = ZBExpCommand(switchLongAddress,
              switchShortAddress,
              0,    //src endpoint
              2,    //dest endpoint
              0x00f0,    //cluster ID
              0xc216, //profile ID
              0,    //broadcast radius
              0x00,    //option
              payload4, //payload
              sizeof(payload4),    //payload length
              myFrameId++);   // frame ID
              xbee.send(tx);
//              Serial.println();
              Serial.print("sent funny hardware message #2 frame ID: ");
              Serial.println(myFrameId-1);
            
        }
        else if (clusterId == 0xf6){
          // This is The Range Test command response.
          Serial.print("Cluster Cmd: ");
          Serial.print(rx.getRFData()[2],HEX);
          Serial.print(" ");
          Serial.print("*** Cluster ID 0xf6 ");
          if (rx.getRFData()[2] == 0xfd){
            Serial.print("RSSI value: ");
            print8Bits(rx.getRFData()[3]);
            Serial.print(" ");
            print8Bits(rx.getRFData()[4]);
            Serial.print(" ");
            Serial.print((int8_t)rx.getRFData()[3]);
            Serial.println();
          }
          else if (rx.getRFData()[2] == 0xfe){
            Serial.println("Version information");
            // bytes 0 - 2 are the packet overhead
            // This can be decoded to give the data from the switch,
            // but frankly, I didn't know what I would do with it 
            // once decoded, so I just skip it.
          }
          // This is to catch anything that may pop up in testing
          else{
            Serial.print(rx.getRFData()[2],HEX);
            Serial.print(" ");
            for(int i=0; i < rx.getRFDataLength(); i++){
              print8Bits(rx.getRFData()[i]);
              Serial.print(' ');
            }
            Serial.println();
          }
        }
        if (clusterId == 0x00f0){
//          Serial.println("Most likely a temperature reading; useless");
//          for(int i=0; i < rx.getRFDataLength(); i++){
//              print8Bits(rx.getRFData()[i]);
//              Serial.print(' ');
//          }
//          Serial.println();
          Serial.print("Cluster Cmd: ");
          Serial.print(rx.getRFData()[2],HEX);
          Serial.print(" ");
          uint16_t count = (uint8_t)rx.getRFData()[5] + 
              ((uint8_t)rx.getRFData()[6] << 8);
          Serial.print("Count: ");
          Serial.print(count);
          Serial.print(" ");
          Serial.print(rx.getRFData()[12]);
          Serial.print(" ");
          Serial.print(rx.getRFData()[13]);
          Serial.print(" ");
          uint16_t temp =  (uint8_t)rx.getRFData()[12] + 
              ((uint8_t)rx.getRFData()[13] << 8);
          Serial.println(temp);
//          temp = (temp / 1000) * 9 / 5 + 32;
//          Serial.println(temp);

        }
        if (clusterId == 0x00ef){
          //
          // This is a power report, there are two kinds, instant and summary
          //
          Serial.print("Cluster Cmd: ");
          Serial.print(rx.getRFData()[2],HEX);
          Serial.print(" ");
          Serial.print("*** Power Data, ");
          // The first byte is what Digi calls 'Frame Control'
          // The second is 'Transaction Sequence Number'
          // The third is 'Command ID'
          if (rx.getRFData()[2] == 0x81){
            // this is the place where instant power is sent
            // but it's sent 'little endian' meaning backwards
            int power = rx.getRFData()[3] + (rx.getRFData()[4] << 8);
            Serial.print("Instantaneous Power is: ");
            Serial.println(power);
          }
          else if (rx.getRFData()[2] == 0x82){
            unsigned long minuteStat = (uint32_t)rx.getRFData()[3] + 
              ((uint32_t)rx.getRFData()[4] << 8) + 
              ((uint32_t)rx.getRFData()[5] << 16) + 
              ((uint32_t)rx.getRFData()[6] << 24);
            unsigned long uptime = (uint32_t)rx.getRFData()[7] + 
              ((uint32_t)rx.getRFData()[8] << 8) + 
              ((uint32_t)rx.getRFData()[9] << 16) + 
              ((uint32_t)rx.getRFData()[10] << 24);
            int resetInd = rx.getRFData()[11];
            Serial.print("Minute Stat: ");
            Serial.print(minuteStat);
            Serial.print(" watt seconds, Uptime: ");
            Serial.print(uptime);
            Serial.print(" seconds, Reset Ind: ");
            Serial.println(resetInd);
          }
        }
        if (clusterId == 0x00ee){
          //
          // This is where the current status of the switch is reported
          //
          // If the 'cluster command' is 80, then it's a report, there
          // are other cluster commands, but they are controls to change
          // the switch.  I'm only checking the low order bit of the first
          // byte; I don't know what the other bits are yet.
          if (rx.getRFData()[2] == 0x80){
            Serial.print("Cluster Cmd: ");
            Serial.print(rx.getRFData()[2],HEX);
            Serial.print(" ");
            if (rx.getRFData()[3] & 0x01)
              Serial.println("Light switched on");
            else
              Serial.println("Light switched off");
          }
        }
      }
      else if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
        ZBTxStatusResponse txStatus;
        xbee.getResponse().getZBTxStatusResponse(txStatus);
        Serial.print("Status Response: ");
        Serial.println(txStatus.getDeliveryStatus(), HEX);
        Serial.print("To Frame ID: ");
        Serial.println(txStatus.getFrameId());
      }
      else {
        Serial.print("Got frame type: ");
        Serial.println(xbee.getResponse().getApiId(), HEX);
      }
    }
    else if (xbee.getResponse().isError()) {
      // some kind of error happened, I put the stars in so
      // it could easily be found
      Serial.print("************************************* error code:");
      Serial.println(xbee.getResponse().getErrorCode(),DEC);
    }
    else {
      // I hate else statements that don't have some kind
      // ending.  This is where you handle other things
    }
    if (Serial.available() > 0) {
      char incomingByte;
      
      incomingByte = Serial.read();
      Serial.print("Selection: ");
      Serial.println(atoi(&incomingByte), DEC);
      // This message set will turn the light off
      if (atoi(&incomingByte) == 0){
        uint8_t payload1[] = {0x01}; //
        uint8_t payloadOff[] = {0x00,0x01};
        sendSwitch(0x00, 0x02, 0x00ee, 0xc216, 0x01, payload1, sizeof(payload1));
        sendSwitch(0x00, 0x02, 0x00ee, 0xc216, 0x02, payloadOff, sizeof(payloadOff));
      }
      // This pair of messages turns the light on
      else if (atoi(&incomingByte) == 1){
        uint8_t payload1[] = {0x01}; //
        uint8_t payloadOn[] = {0x01,0x01};
        sendSwitch(0x00, 0x02, 0x00ee, 0xc216, 0x01, payload1, sizeof(payload1));
        sendSwitch(0x00, 0x02, 0x00ee, 0xc216, 0x02, payloadOn, sizeof(payloadOn));
      }
      // this goes down to the test routine for further hacking
      else if (atoi(&incomingByte) == 2){
        testCommand();
      }
      // This will get the Version Data, it's a combination of data and text
      else if (atoi(&incomingByte) == 3){
        uint8_t data[] = {0x00, 0x01};
        sendSwitch(0x00, 0x02, 0x00f6, 0xc216, 0xfc, data, sizeof(data));
      }
      // This command causes a message return holding the state of the switch
      else if (atoi(&incomingByte) == 4){
        uint8_t data[] = {0x01};
        sendSwitch(0x00, 0x02, 0x00ee, 0xc216, 0x01, data, sizeof(data));
      }
      // restore normal mode after one of the mode changess that follow
      else if (atoi(&incomingByte) == 5){ 
        uint8_t databytes[] = {0x00, 0x01};
        sendSwitch(0, 0x02, 0x00f0, 0xc216, 0xfa, databytes, sizeof(databytes));
      }
      // range test - periodic double blink, no control, sends RSSI, no remote control
      // remote control works
      else if (atoi(&incomingByte) == 6){ 
        uint8_t databytes[] = {0x01, 0x01};
        sendSwitch(0, 0x02, 0x00f0, 0xc216, 0xfa, databytes, sizeof(databytes));
      }
      // locked mode - switch can't be controlled locally, no periodic data
      else if (atoi(&incomingByte) == 7){ 
        uint8_t databytes[] = {0x02, 0x01};
        sendSwitch(0, 0x02, 0x00f0, 0xc216, 0xfa, databytes, sizeof(databytes));
      }
      // Silent mode, no periodic data, but switch is controllable locally
      else if (atoi(&incomingByte) == 8){ 
        uint8_t databytes[] = {0x03, 0x01};
        sendSwitch(0, 0x02, 0x00f0, 0xc216, 0xfa, databytes, sizeof(databytes));
      }
    }
    Alarm.delay(0); // Just for the alarm routines

}

uint8_t testValue = 0x00;

void testCommand(){
  Serial.println("testing command");
  return;
  Serial.print("Trying value: ");
  Serial.println(testValue,HEX);
  uint8_t databytes[] = {};
  sendSwitch(0, 0xf0, 0x0b7d, 0xc216, testValue++, databytes, sizeof(databytes));
  if (testValue != 0xff)
    Alarm.timerOnce(1,testCommand); // try it again in a second
}

/*
  Because it got so cumbersome trying the various clusters for various commands,
  I created this send routine to make things a little easier and less prone to 
  typing mistakes.  It also made the code to implement the various commands I discovered
  easier to read.
*/
void sendSwitch(uint8_t sEndpoint, uint8_t dEndpoint, uint16_t clusterId,
        uint16_t profileId, uint8_t clusterCmd, uint8_t *databytes, int datalen){
          
  uint8_t payload [10];
  ZBExpCommand tx;
//  Serial.println("Sending command");
  //
  // The payload in a ZigBee Command starts with a frame control field
  // then a sequence number, cluster command, then databytes specific to
  // the cluster command, so we have to build it up in stages
  // 
  // first the frame control and sequence number
  payload[0] = 0x11;
  payload[1] = 0;
  payload[2] = clusterCmd;
  for (int i=0; i < datalen; i++){
    payload[i + 3] = databytes[i];
  }
  int payloadLen = 3 + datalen;
  // OK, now we have the ZigBee cluster specific piece constructed and ready to send
  
  tx = ZBExpCommand(switchLongAddress,
    switchShortAddress,
    sEndpoint,    //src endpoint
    dEndpoint,    //dest endpoint
    clusterId,    //cluster ID
    profileId, //profile ID
    0,    //broadcast radius
    0x00,    //option
    payload, //payload
    payloadLen,    //payload length
    myFrameId++);   // frame ID
    
    xbee.send(tx);
    Serial.print("sent command: ");
    Serial.print(payload[2], HEX);
    Serial.print(" frame ID: ");
    Serial.println(myFrameId-1);
}

/*-------------------------------------------------------*/
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
