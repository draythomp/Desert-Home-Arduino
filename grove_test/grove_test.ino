/*

 Arduino GroveStreams Stream Feed via Ethernet
 
 The GroveStreams client sketch is designed for the Arduino and Ethernet.
 A full "how to" guide for this sketh can be found at https://www.grovestreams.com/developers/getting_started_arduino_temp.html
 This sketch updates several stream feeds with an analog input reading,
 from a temperature probe, via the GroveStreams API: https://www.grovestreams.com/developers/apibatchfeed.html
 The Arduino uses DHCP and DNS for a simpler network setup.
 The sketch also includes a Watchdog / Reset function to make sure the
 Arduino stays connected and/or regains connectivity after a network outage.
 Use the Serial Monitor on the Arduino IDE to see verbose network feedback
 and GroveStreams connectivity status.
 
 GroveStreams Setup:
 
 * Sign Up for Free User Account - https://www.grovestreams.com
 * Create a GroveStreams organization and select the Arduino blueprint
 * Enter a unique MAC Address for this network in this sketch under "Local Network Settings" 
 *    (Newer shields have the mac address on a sticker on the shield. Use that.)
 *    (A MAC address can also be generated within a GroveStreams organization: tools - Generate MAC Address)
 * Enter the GroveStreams org uid under "GroveStreams Settings" in this sketch 
 *    (Can be retrieved from a GroveStreams organization: tools - View Organization UID)
 * Enter the GroveStreams api key under "GroveStreams Settings" in this sketch  
 *    (Can be retrieved from a GroveStreams organization: click the Api Keys toolbar button, 
 *     select your Api Key, and click View Secret Key)
 
 Arduino Requirements:
 
 * Arduino with Ethernet Shield or Arduino Ethernet
 * Arduino 1.0 IDE
 
 Network Requirements:
 
 * Ethernet port on Router    
 * DHCP enabled on Router
 * Unique MAC Address for Arduino
 
 Additional Credits:
 Example sketches from Arduino team, Ethernet by David A. Mellis
 
 */

#include <SPI.h>
#include <Ethernet.h>
#include <MemoryFree.h>


// Local Network Settings
byte mac[] = {0x90, 0xA2, 0xDA, 0x00, 0x33, 0x33};        // Change this!!! Must be unique on local network. 
                                               // Look for a sticker on the back of your Ethernet shield.

// GroveStreams Settings
String gsApiKey = "e259131d-27fc-30b2-82c2-f671bc2e2e8c";   //Change This!!!
String gsOrg = "eb2212c7-94ac-3286-8dc8-56922f26ab05";      //Change This!!!
String gsComponentName = "Temperature";                     //Optionally change. Set this to give your component a name when it initially registers.

char gsDomain[] = "grovestreams.com";   //Don't change. The Grove Streams domain. 
String gsComponentTemplateId = "temp";  //Don't change. Tells GS what template to use when the feed initially arrives and a new component needs to be created.
                                        // The blueprint is expecting "temp".

//GroveStreams Stream IDs. Stream IDs tell GroveStreams which component streams the values will be assigned to.
//Don't change these unless you edit your GroveStreams component definition and change the stream ID to match this.
String gsStreamId1 = "s1";   //Temp C - Random Stream. 
String gsStreamId2 = "s2";   //Temp F - Random Stream. Don't change.
String gsStreamId3 = "s3";   //Temp C - Interval Stream (20 second intervals). Don't change.
String gsStreamId4 = "s4";   //Temp F - Interval Stream (20 second Intervals). Don't change.

const int updateFrequency = 20 * 1000; // GroveStreams update frequency in milliseconds (the GS blueprint is expecting 20s)
const int temperaturePin = 0;          // Then Temperature pin number.    

// Variable Setup
String myIPAddress;  //Set below from DHCP. Needed by GroveStreams to verify that a device is not uploading more than once every 10s.
String myMac;        //Set below from the above mac variable. The readable Mac is used by GS to determine which component the feeds are uploading into.

long lastConnectionTime = 0;    //Don't change. Used to determine if the Ethernet needs to be reconnected. 
boolean lastConnected = false;  //Don't change. 
int failedCounter = 0;          //Don't change. 

// Initialize Arduino Ethernet Client
EthernetClient client;


void setup()
{
  // Start Serial for debugging on the Serial Monitor
  Serial.begin(9600);

  // Start Ethernet on Arduino
  startEthernet();
  randomSeed(analogRead(1));
}

void loop()
{

  // Print Update Response to Serial Monitor
  if (client.available())
  {
    char c = client.read();
    Serial.print(c);
  }

  // Disconnect from GroveStreams
  if (!client.connected() && lastConnected)
  {
    Serial.println("...disconnected");
    Serial.println();
    showMem();

    client.stop();
  }

  // Update sensor data to GroveStreams
  if(!client.connected() && (millis() - lastConnectionTime > updateFrequency))
  {
    String tempC = getTemperatureC();
    Serial.print (tempC);
    String tempF = getTemperatureF();
    Serial.print (tempF);
    updateGroveStreams(tempC, tempF);
  }

  // Check if Arduino Ethernet needs to be restarted
  if (failedCounter > 3 ) {
    
    //Too many failures. Restart Ethernet.
    startEthernet();
  }

  lastConnected = client.connected();
}

void updateGroveStreams(String tempC, String tempF)
{
  Serial.println("\nin update routine");
  if (client.connect(gsDomain, 80))
  {         

    //Assemble the url used to pass the temperature readings to GroveStreams
    // The Arduino String class contains many memory bugs and char arrays should be used instead, but
    // to make this example simple to understand we have chosen to use the String class. 
    // No none memory issues have been seen with this example to date.
    //We are passing temperature readings into two types of GroveStreams streams, Random and Interval streams.
    String url = "PUT /api/feed?&compTmplId=" + gsComponentTemplateId + "&compId=" + myMac + "&compName=" + gsComponentName;
    url += "&org=" + gsOrg + "&api_key=" + gsApiKey;

    url += "&" + gsStreamId1 + "=" + tempC;  //Temp C - Random Stream
    url += "&" + gsStreamId2 + "=" + tempF;  //Temp F - Random Stream
    url += "&" + gsStreamId3 + "=" + tempC;  //Temp C - Interval Stream (20 second intervals)
    url += "&" + gsStreamId4 + "=" + tempF;  //Temp F - Interval Stream (20 second intervals)

    url += " HTTP/1.1";
    Serial.print(url);
    client.println(url);  //Send the url with temp readings in one println(..) to decrease the chance of dropped packets
    client.println("Host: " + String(gsDomain));
    client.println("Connection: close");
    client.println("X-Forwarded-For: "+ myIPAddress); //Include this line if you have more than one device uploading behind 
                                                      // your outward facing router (avoids the GS 10 second upload rule)
    client.println("Content-Type: application/json");
    client.println();

    if (client.available())
    {
      //Read the response and display in the the console
      char c = client.read();
      Serial.print(c);
    }

    lastConnectionTime = millis();

    if (client.connected())
    {     
      failedCounter = 0;
    }
    else
    {
      //Connection failed. Increase failed counter
      failedCounter++;

      Serial.println("Connection to GroveStreams failed ("+String(failedCounter, DEC)+")");   
      Serial.println();
    }

  }
  else
  {
     //Connection failed. Increase failed counter
    failedCounter++;

    Serial.println("Connection to GroveStreams Failed ("+String(failedCounter, DEC)+")");   
    Serial.println();

    lastConnectionTime = millis(); 
  }
}

void startEthernet()
{
  //Start or restart the Ethernet connection.
  client.stop();

  Serial.println("Connecting Arduino to network...");
  Serial.println();  

  //Wait for the connection to finish stopping
  delay(1000);

  //Connect to the network and obtain an IP address using DHCP
  if (Ethernet.begin(mac) == 0)
  {
    Serial.println("DHCP Failed, reset Arduino to try again");
    Serial.println();
  }
  else
  {

    Serial.println("Arduino connected to network using DHCP");
    Serial.println();

    //Wait to ensure the connection finished
    delay(1000);

    //Set the mac and ip variables so that they can be used during sensor uploads later
    myMac =getMacReadable();
    Serial.println("MAC: " + myMac);

    myIPAddress = getIpReadable(Ethernet.localIP());
    Serial.println("IP address: " + myIPAddress);
  }

}

String getMacReadable() 
{
  //Convert the mac address to a readable string
  char macstr[20];
  snprintf(macstr, 100, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macstr);
}

String getIpReadable(IPAddress p)
{
  //Convert the ip address to a readable string
  String ip;
  for (int i =0; i < 3; i++)
  {
    ip += String(p[i], DEC);
    ip += ".";
  }
  ip +=String(p[3], DEC);
  return ip;
}

String getTemperatureF() 
{
  //Get the temperature analog reading and convert it to a string
  float voltage, degreesC, degreesF; 

  //voltage = (analogRead(temperaturePin) * 0.004882814);
  //degreesF = degreesC * (9.0/5.0) + 32.0;
  degreesF = float(random(70, 120));

  char temp[20] = {0}; //Initialize buffer to nulls
  dtostrf(degreesF, 12, 3, temp); //Convert float to string

  String stemp = temp;
  stemp.trim();  //Trim off head and tail spaces
  return stemp; 
}

String getTemperatureC() 
{
  //Get the temperature analog reading and convert it to a string
  float voltage, degreesC, degreesF; 

  //voltage = (analogRead(temperaturePin) * 0.004882814);
  //degreesC = (voltage - 0.5) * 100.0;
  degreesC = float(random(20, 40));

  char temp[20] = {0}; //Initialize buffer to nulls
  dtostrf(degreesC, 12, 3, temp); //Convert float to string

  String stemp = temp;
  stemp.trim();  //Trim off head and tail spaces
  return stemp; 
}

void showMem(){
  char Dbuf [100];
  
  strcpy_P(Dbuf,PSTR("Mem = "));
  Serial.print(Dbuf);
  Serial.println(freeMemory());
}

