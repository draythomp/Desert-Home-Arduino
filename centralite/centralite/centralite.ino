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
  setTime(0,0,0,1,1,14);  // just so alarms work well, I don't really need the time.
  Serial.println("started");
}

void loop() {
    // doing the read without a timer makes it non-blocking, so
    // you can do other stuff in loop() as well.  Things like
    // looking at the console for something to turn the switch on
    // or off (see waaay down below)
    xbee.readPacket();
    // the read above will set the available up to 
    // work when you check it.
    if (xbee.getResponse().isAvailable()) {
      // got something
      Serial.println();
      Serial.print("Frame Type is ");
      // Andrew called the XBee frame type ApiId, it's the first byte
      // of the frame specific data in the packet.
      int frameType = xbee.getResponse().getApiId();
      Serial.println(frameType, HEX);
      //
      // All ZigBee device interaction is handled by the two XBee message type
      // ZB_EXPLICIT_RX_RESPONSE (ZigBee Explicit Rx Indicator Type 91)
      // ZB_EXPLICIT_TX_REQUEST (Explicit Addressing ZigBee Command Frame Type 11)
      // This test code only uses these and the Transmit Status message
      //
      if (frameType == ZB_EXPLICIT_RX_RESPONSE) {
        // now that you know it's a Zigbee receive packet
        // fill in the values
        xbee.getResponse().getZBExpRxResponse(rx);
        int senderProfileId = rx.getProfileId();
        Serial.print(F(" Profile ID: "));
        Serial.print(senderProfileId, HEX);
        
        // get the 64 bit address out of the incoming packet so you know 
        // which device it came from
        Serial.print(" from: ");
        XBeeAddress64 senderLongAddress = rx.getRemoteAddress64();
        print32Bits(senderLongAddress.getMsb());
        Serial.print(" ");
        print32Bits(senderLongAddress.getLsb());
        
        // this is how to get the sender's
        // 16 bit address and show it
        uint16_t senderShortAddress = rx.getRemoteAddress16();
        Serial.print(" (");
        print16Bits(senderShortAddress);
        Serial.println(")");
        
        // for right now, since I'm only working with one switch
        // save the addresses globally for the entire test module
        switchLongAddress = rx.getRemoteAddress64();
        switchShortAddress = rx.getRemoteAddress16();

        uint8_t* frameData = rx.getFrameData();
        // We're working with a message specifically designed for the
        // ZigBee protocol, see the XBee documentation to get the layout
        // of the message.
        Serial.print(F(" Cluster ID: "));
        uint16_t clusterId = (rx.getClusterId());
        print16Bits(clusterId);
        //
        // I have the message and it's from a ZigBee device
        // so I have to deal with things like cluster ID, Profile ID
        // and the other strangely named fields that these devices use
        // for information and control
        //
        // For example, cluster IDs have different uses under different profiles
        if (senderProfileId == 0x0000){  // This is the general profile
          if (clusterId == 0x00){
            Serial.println(F(" Basic Cluster"));
          }
          else if (clusterId == 0x0006){ // Match Descriptor
            Serial.println(F(" Match Descriptor"));
            /*************************************/
            // I don't actually need this message, but it comes in as soon as
            // a device is plugged in.  I answer it with a messsage that says I
            // have an input cluster 0x19, since that's what it's looking for.
            // Ignoring this message doesn't seem to hurt anything either.
            uint8_t mdPayload[] = {0xAA,0x00,0x00,0x00,0x01,0x19};
            mdPayload[2] = switchShortAddress & 0x00ff;
            mdPayload[3] = switchShortAddress >> 8;
            ZBExpCommand tx = ZBExpCommand(switchLongAddress,
              switchShortAddress,
              0,    //src endpoint
              0,    //dest endpoint
              0x8006,    //cluster ID
              0x0000, //profile ID
              0,    //broadcast radius
              0x00,    //option
              mdPayload, //payload
              sizeof(mdPayload),    //payload length
              0x00);   // frame ID
            xbee.send(tx);
            // if you unplug a device, and then plug it back in, it loses the 
            // configuration for reporting on/off changes.  So, send the configuration
            // to get the switch working the way I want it to.  (see below for 
            // detailed comments.
            Serial.println (F("sending cluster command Configure Reporting "));
            uint8_t crPayload[] = {0x00,0xaa,0x06,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x40,0x00,0x00};
            tx = ZBExpCommand(switchLongAddress,
              switchShortAddress,
              0,    //src endpoint
              1,    //dest endpoint
              0x0006,    //cluster ID
              0x0104, //profile ID
              0,    //broadcast radius
              0x00,    //option
              crPayload, //payload
              sizeof(crPayload),    //payload length
              0x00);   // frame ID
            xbee.send(tx);
            
         }
          else if (clusterId == 0x0013){
            Serial.println(F(" Device Announce Message"));
            switchLongAddress = rx.getRemoteAddress64();
            switchShortAddress = rx.getRemoteAddress16();
            // Ok we saw the switch, now just for fun, get it to tell us
            // what profile it is using and some other stuff.
            // We'll send an Acttive Endpoint Request to do this
            Serial.println (F("sending Active Endpoint Request "));
            // The active endpoint request needs the short address of the device
            // in the payload.  Remember, it needs to be little endian (backwards)
            // The first byte in the payload is simply a number to identify the message
            // the response will have the same number in it.
            uint8_t aePayload[] = {0xAA,0x00,0x00};
            aePayload[1] = switchShortAddress & 0x00ff;
            aePayload[2] = switchShortAddress >> 8;
            ZBExpCommand tx = ZBExpCommand(switchLongAddress,
              switchShortAddress,
              0,    //src endpoint
              0,    //dest endpoint
              0x0005,    //cluster ID
              0x0000, //profile ID
              0,    //broadcast radius
              0x00,    //option
              aePayload, //payload
              sizeof(aePayload),    //payload length
              0xaa);   // frame ID
            xbee.send(tx);
          }
          else if (clusterId == 0x8004){
            Serial.println(F(" Simple Descriptor Response "));
            // Since I've been through this switch a few times, I already know
            // what to expect out of it.  This response is how you get the actual 
            // clusters that it has code for, and the profile ID that it supports.
            // Since this is a light switch, it will support profile 0x104 and have
            // clusters that support things like on/off and reporting.
            // The items of interest are in the rf_data payload, and this is one way
            // to get them out.
            unsigned char *data = rx.getRFData();  // first get a pointer to the data
            Serial.print(F(" Transaction ID: "));
            print16Bits(data[0]); // remember the number that starts the payload?
            Serial.println();
            Serial.print(F(" Endpoint Reported: "));
            print8Bits(data[5]);
            Serial.println();
            Serial.print(F(" Profile ID: "));
            print8Bits(data[7]);  // Profile ID is 2 bytes long little endian (backwards)
            print8Bits(data[6]);
            Serial.println();
            Serial.print(F(" Device ID: "));
            print8Bits(data[9]);  // Device ID is 2 bytes long little endian (backwards)
            print8Bits(data[8]);
            Serial.println();
            Serial.print(F(" Device Version: "));
            print8Bits(data[10]);  // Device ID is 1 byte long
            Serial.println();
            Serial.print(F(" Number of input clusters: "));
            print8Bits(data[11]);  // Input cluster count
            Serial.print(F(", Clusters: "));
            Serial.println();
            for (int i = 0; i < data[11]; i++){
              Serial.print(F("    "));
               print8Bits(data[i*2+13]); // some more of that little endian crap
               print8Bits(data[i*2+12]);
               Serial.println();
            }
            int outidx = 11 + 1 + 2*data[11];
            Serial.print(F(" Number of output clusters: "));
            print8Bits(data[outidx]);  // Input cluster count
            Serial.print(F(", Clusters: "));
            Serial.println();
            for (int i = 0; i < data[outidx]; i++){
              Serial.print(F("    "));
               print8Bits(data[i*2 + outidx + 2]); // some more of that little endian crap
               print8Bits(data[i*2 + outidx + 1]);
               Serial.println();
            }
            Serial.println (F("sending cluster command Configure Reporting "));
            // OK, for illustration purposes, this is enough to actually do something
            // First though, let's set up the switch so that it reports when it has 
            // changed states in the on/off cluster (cluster 0006).  This will require we
            // send a message to the on/off cluster with the "Configure Reporting" command
            // (0x06) with a bunch of parameters to specify things.
            uint8_t crPayload[] = {0x00,0xaa,0x06,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x40,0x00,0x00};
            ZBExpCommand tx = ZBExpCommand(switchLongAddress,
              switchShortAddress,
              0,    //src endpoint
              1,    //dest endpoint
              0x0006,    //cluster ID
              0x0104, //profile ID
              0,    //broadcast radius
              0x00,    //option
              crPayload, //payload
              sizeof(crPayload),    //payload length
              0x00);   // frame ID
            xbee.send(tx);

          } 
          else if (clusterId == 0x8005){
            Serial.println(F(" Active Endpoints Response"));
            // This message tells us which endpoint to use
            // when controlling the switch.  Since this is only a switch,
            // it will give us back one endpoint.  I should really have a loop
            // in here to handle multiple endpoints, but ...
            Serial.print(F("  Active Endpoint Count reported: "));
            Serial.println(rx.getRFData()[4]);
            Serial.print(F("  Active Endpoint: "));
            Serial.println(rx.getRFData()[5]);
            // Now we know that it has an endpoint, but we don't know what profile
            // the endpoint is under.  So, we send a Simple Descriptor Request to get
            // that.
            Serial.println (F("sending Simple Descriptor Request "));
            // The request needs the short address of the device
            // in the payload.  Remember, it needs to be little endian (backwards)
            // The first byte in the payload is simply a number to identify the message
            // the response will have the same number in it.  The last number is the
            // endpoint we got back in the Active Endpoint Response.  
            // Also note that we're still dealing with profile 0 here, we haven't gotten
            // to the device we actually want to play with yet.
            uint8_t sdPayload[] = {0xAA,0x00,0x00,01};
            sdPayload[1] = switchShortAddress & 0x00ff;
            sdPayload[2] = switchShortAddress >> 8;
            sdPayload[3] = rx.getRFData()[5];
            ZBExpCommand tx = ZBExpCommand(switchLongAddress,
              switchShortAddress,
              0,    //src endpoint
              0,    //dest endpoint
              0x0004,    //cluster ID
              0x0000, //profile ID
              0,    //broadcast radius
              0x00,    //option
              sdPayload, //payload
              sizeof(sdPayload),    //payload length
              0xaa);   // frame ID
            xbee.send(tx);
          }
          else{
            Serial.println(F(" Haven't implemented this cluster yet"));
          }
        }
        else if(senderProfileId == 0x0104){   // This is the Home Automation profile
          // Since these are all ZCL (ZigBee Cluster Library) messages, I'll suck out
          // the cluster command, and payload so they can be used easily.
          Serial.println();
          Serial.print(" RF Data Received: ");
          for(int i=0; i < rx.getRFDataLength(); i++){
              print8Bits(rx.getRFData()[i]);
              Serial.print(' ');
          }
          Serial.println();
          if (clusterId == 0x0000){
            Serial.print(F(" Basic Cluster"));
          }
          else if (clusterId == 0x0006){ // Switch on/off 
            Serial.println(F(" Switch on/off"));
            // with the Centralite switch, we don't have to respond
            // A message to this cluster tells us that the switch changed state
            // However, if the response hasn't been configured, it will give back 
            // default response (cluster command 0b)
            // so let's dig in and see what's going on.
            //
            // The first two bytes of the rfdata are the ZCL header, the rest of
            // the data is a three field indicator of the attribute that changed
            // two bytes of attribute identifier, a byte of datatype, and some bytes
            // of the new value of the attribute.  Since this particular attribute is a 
            // boolean (on or off), there will only be one byte.  So
            if(rx.getRFData()[2] == 0x0b){ // default response
              Serial.println(F("  Default Response: "));
              Serial.print(F("  Command: "));
              print8Bits(rx.getRFData()[3]);  
              Serial.println();
              Serial.print(F("  Status: "));
              print8Bits(rx.getRFData()[4]);  
              Serial.println();
            }
            else if (rx.getRFData()[2] == 0x0a){
              Serial.print(F("  Attribute: "));
              print8Bits(rx.getRFData()[4]);  // never, ever forget little endian
              print8Bits(rx.getRFData()[3]);
              Serial.println();
              Serial.print(F("  Datatype: "));
              print8Bits(rx.getRFData()[5]);
              Serial.print(F("  (0x10 means Boolean) "));
              Serial.println();
              Serial.print(F("  Value: "));
              print8Bits(rx.getRFData()[6]);
              Serial.println();
            }
            else{
              Serial.print(F("  I don't know what this is"));
            }
          }
          else{
            Serial.println(F(" Haven't implemented this cluster yet"));
          }
        }
      }
      else {
        Serial.print(F("Got frame type: "));
        Serial.print(frameType, HEX);
        if (frameType == 0xa1)
          Serial.println(F(" Route Record Request"));
        else if (frameType == ZB_TX_STATUS_RESPONSE)
          Serial.print(F(" Transmit Status Response"));
        else
          Serial.println(F(" I didn't implement this frame type yet"));
      }
    }
    else if (xbee.getResponse().isError()) {
      // some kind of error happened, I put the stars in so
      // it could easily be found
      Serial.print("************************************* error code:");
      Serial.println(xbee.getResponse().getErrorCode(),DEC);
    }
    else {
      // If you get here it only means you haven't collected enough bytes from
      // the XBee to compose a packet.
    }
  // After checking the XBee for data, look at the serial port
  if (Serial.available() > 0) {
    char incomingByte;
    
    incomingByte = Serial.read();
    if (isdigit(incomingByte)){
      Serial.print("Selection: ");
      Serial.println(atoi(&incomingByte), DEC);
      // switch off
      if (atoi(&incomingByte) == 0){
        Serial.println(F("Turn switch off"));
            uint8_t offPayload[] = {0x11,0x01,0x00};
            ZBExpCommand tx = ZBExpCommand(switchLongAddress,
              switchShortAddress,
              0,    //src endpoint
              1,    //dest endpoint
              0x0006,    //cluster ID
              0x0104, //profile ID
              0,    //broadcast radius
              0x00,    //option
              offPayload, //payload
              sizeof(offPayload),    //payload length
              0x00);   // frame ID
            xbee.send(tx);
      }
      // switch on
      else if (atoi(&incomingByte) == 1){
        Serial.println(F("Turn switch on"));
            uint8_t onPayload[] = {0x11,0xaa,0x01};
            ZBExpCommand tx = ZBExpCommand(switchLongAddress,
              switchShortAddress,
              0,    //src endpoint
              1,    //dest endpoint
              0x0006,    //cluster ID
              0x0104, //profile ID
              0,    //broadcast radius
              0x00,    //option
              onPayload, //payload
              sizeof(onPayload),    //payload length
              0x00);   // frame ID
            xbee.send(tx);
      }
      // switch toggle
      else if (atoi(&incomingByte) == 2){
        Serial.println(F("Toggle switch"));
            uint8_t togglePayload[] = {0x11,0xaa,0x02};
            ZBExpCommand tx = ZBExpCommand(switchLongAddress,
              switchShortAddress,
              0,    //src endpoint
              1,    //dest endpoint
              0x0006,    //cluster ID
              0x0104, //profile ID
              0,    //broadcast radius
              0x00,    //option
              togglePayload, //payload
              sizeof(togglePayload),    //payload length
              0x00);   // frame ID
            xbee.send(tx);
      }
      else {
        Serial.println(F("Try again"));
      }
      // Now a short delay combined with a character read
      // to empty the input buffer.  The IDE developers removed
      // the input flush that used to work for this.
      while(Serial.available() > 0){
        char t = Serial.read();
        delay(25);
      }
    }
  }
  Alarm.delay(0); // Just for the alarm routines
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
