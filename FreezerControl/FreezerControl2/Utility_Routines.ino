/*
Yep, this is all there is to reading a 18B20 temperature sensor

Well, of course there's two libraries involved with hundreds of lines of code to 
support this, but who's counting?
*/

float readTemp(){
  sensors.requestTemperatures(); // Send the command to get temperatures
  float tempF = sensors.getTempFByIndex(0);
  return(tempF);
}

