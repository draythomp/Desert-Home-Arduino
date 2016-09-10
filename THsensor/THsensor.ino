#include "DHT.h"
#include <Wire.h>  // Comes with Arduino IDE
#define DHTPIN 2     // what digital pin we're connected to
#include <LiquidCrystal_I2C.h>

#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

// initialize the temp humidity sensor
DHT dht(DHTPIN, DHTTYPE);
// Now the liquid crystal display (SPI serial)
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

void setup() {
  Serial.begin(9600);
  Serial.println("init the temp humidity sensor!");
  dht.begin();
  Serial.println("init the lcd display");
  lcd.begin(16,2);   // initialize the lcd for 16 chars 2 lines, turn on backlight
  Serial.println("Should be all done now");
  lcd.print("Wait");
}

int savedf = 0;
int savedh = 0;

void loop() {
  char buffer[10];
  
  // Wait a few seconds between measurements.
  delay(2000);

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);
  
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.print(f);
  Serial.print(" *F\t");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.print(" *C ");
  Serial.print(hif);
  Serial.println(" *F");

  if ((int)savedf != (int)(f+0.5)){
    lcd.setCursor(0,0); //Start at character 0 on line 0
    lcd.print("                ");
    lcd.setCursor(0,0); 
    lcd.print("Temperature ");
    lcd.print(dtostrf(f,3,0,buffer));
    lcd.print(char(223));
  }
  if ((int)savedh != (int)(h+0.5)){
    lcd.setCursor(0,1);
    lcd.print("                ");
    lcd.setCursor(0,1);
    lcd.print("Humidity    ");
    lcd.print(dtostrf(h,3,0,buffer));
    lcd.print('%');
  }
  savedh = (int)(h+0.5);
  savedf = (int)(f+0.5);
 
}
