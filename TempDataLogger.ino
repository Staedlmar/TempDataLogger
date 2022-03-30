/*

ToDo
- add poti zum temp sollwert einstellen
- add anzeige ob Heizung an oder aus ist
- add RTC timer for time in log file
- add writing to log file (append mode, brauche ich das?)
  - overwrite if file exists..?
*/

// REQUIRES the following Arduino libraries:
// - DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
// - Adafruit Unified Sensor Lib: https://github.com/adafruit/Adafruit_Sensor

#include "DHT.h"           // for temperature sensor
#include "LiquidCrystal.h" // for LCD display
#include <SPI.h>           // for SD card
#include <SD.h>            // for SD card
//#include <RTCZero.h>       // for getting the current time

// ######################### set I/O's ##############################################
#define DHTPIN 2         // Digital pin connected to the DHT sensor (inside temp)
#define DHTPIN8 8        // Digital pin connected to the DHT sensor (outside temp)

const int relaisIN2 = 3; // pin for relais

const int TempSetUp = 11;
const int TempSetDown = 12;

// pins for LCD Display
const int rs = 10;
const int en = 9;
const int d4 = 22; 
const int d5 = 24; 
const int d6 = 26; 
const int d7 = 28;

// ######################### prepare Temp sensor (DHT) ##################################

#define DHTTYPE DHT22   // DHT 22 (the DHT11 is the low presission one)

// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.

DHT dht(DHTPIN, DHTTYPE);
DHT dht2(DHTPIN8, DHTTYPE);

int count = 0; // counter to switch LCD display between inside and outside temp
//single TempSet = 20.0; // temperature setpoint

// ######################### prepare LCD display ###########################
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// ######################### prepare SD Card unit ##########################
int LoggerCount = 0; // count to make logging to SD card less often
const int LoggerThrsh = 150; // ToDo: make this relavive to the 2s time the dispaly is refreshed
                             //       we want the SD card tro be updated only every 5min
char filename[] = "TempLog.csv"; // only 8 characters are allowed

// #########################################################################
// ########################     i n i t     ################################
// #########################################################################
void setup() {
  Serial.begin(9600);

  //rtc.begin(); // initialize RTC

  // Temp sensor init
  dht.begin();  
  dht2.begin();  

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("starting...");

  // set relais output adn test relais
  pinMode(relaisIN2, OUTPUT);
  digitalWrite(relaisIN2, HIGH);  //Relais2 an
  delay(1000);
  digitalWrite(relaisIN2, LOW);  //Relais2 aus

  // set input for temperature setpoint
  pinMode(TempSetUp, INPUT);
  pinMode(TempSetDown, INPUT);

  // wait for SD module to start 
  pinMode (53, OUTPUT);
  
  if (!SD.begin(53)) 
  {
    lcd.setCursor(0, 0);
    lcd.print("No SD Module Detected");
    while (1);
  } 
  else
  {
    lcd.setCursor(0, 0);
    lcd.print("SD initalized!");
  }

  delay(1000);
  File dataFile = SD.open(filename, FILE_WRITE);
  if (dataFile) 
  {
    dataFile.println("=================================================================");
    dataFile.println("         Temperature Datalogger by Martin Staedler (2022) ");
    dataFile.println("=================================================================");
    dataFile.println("");
    int bytesWritten = dataFile.println("In Temp [deg], In Humitity [%], Out Temp [deg], Out Humitity [%] ");
    dataFile.close();
    Serial.print("Bytes written to SD card: ");  // Debugging
    Serial.println(bytesWritten);                // Debugging
  }
  else
  {
     Serial.println("error opening datalog file");
  }
}

// #########################################################################
// ########################     l o o p     ################################
// #########################################################################
void loop() {
  // Wait a few seconds between measurements.
  delay(2000);

  // ####################### Temp mesaurement #####################
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor (inside)!"));
    return;
  }

  float h_out = dht2.readHumidity();
  // Read temperature as Celsius (the default)
  float t_out = dht2.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(h_out) || isnan(t_out)) {
    Serial.println(F("Failed to read from DHT sensor (outside)!"));
    return;
  }

  // ####################### Temp Setpoint #####################
  if (digitalRead(TempSetDown)) 
    {
    delay(50); // entprellen
    {
    if (digitalRead(TempSetDown)) 
    {
     // TempSet = TempSet + 0.5;
      // ToDo: setpoint anzeigen solange ther taaster gedrückt ist, oder lange drücken um in den set mode zu kommen?
    }
  }
    }

  // ToDo: das ganze für temp down, Function bauen?

  // ####################### Temp Control ######################
  // switch ON/OFF heating
  //char Hzg[4] = "OFF";
  if (t > 28.0)
  {
    // schalte Heizung AUS
    Serial.print(F("Heizung aus "));
    //Hzg = {'O', 'F', 'F'};
    digitalWrite(relaisIN2, LOW);  //Relais2 aus
  }
  if (t < 22.0)
  {
    // schalte Heizung EIN
    Serial.print(F("Heizung ein "));
    //Hzg = "ON ";
    digitalWrite(relaisIN2, HIGH);  //Relais2 an
  }

  // ####################### Display #####################
  count++;
  if (count <= 2)
  {
    // print to LCD display
    lcd.setCursor(0, 0);
    lcd.print("                "); //Print blanks to clear the row
    lcd.setCursor(0, 0);
    lcd.print("IN  Temp:");
    lcd.print(t);
    lcd.print("\337C "); // grad Celsuis
    //lcd.print(Hzg);      // heating ON/OFF

    lcd.setCursor(0, 1);
    lcd.print("                "); //Print blanks to clear the row
    lcd.setCursor(0, 1);
    lcd.print("OUT Temp:");
    lcd.print(t_out);
    lcd.print("\337C "); // grad Celsuis
  }
  if (count > 2)
  {
    lcd.setCursor(0, 0);
    lcd.print("                "); //Print blanks to clear the row
    lcd.setCursor(0, 0);
    lcd.print("IN  Feucht:");
    lcd.print(h);
    //lcd.print(Hzg);      // heating ON/OFF

    lcd.setCursor(0, 1);
    lcd.print("                "); //Print blanks to clear the row
    lcd.setCursor(0, 1);
    lcd.print("OUT Feucht:");
    lcd.print(h_out);
    if (count >= 4)
      count = 0;
  }

  // ##################### Looging to SD card ####################
  // ToDo: get time using RTC
  // 
  LoggerCount++;
  if (LoggerCount > LoggerThrsh)
  {
    File dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile) 
    {
      dataFile.print(t);
      dataFile.print(", ");
      dataFile.print(h);
      dataFile.print(", ");
      dataFile.print(t_out);
      dataFile.print(", ");
      dataFile.print(h_out);
      int bytesWritten = dataFile.println(", ");
      dataFile.close();
      Serial.print("Bytes written to SD (loop) card: ");  // Debugging
      Serial.println(bytesWritten);                // Debugging
    }
    else
    {
      Serial.println("error opening datalog file");
    }
    LoggerCount = 0;
  }

  // #################### debugging ##############################
  //                    serial monitor
  // #############################################################
  Serial.print(F("Humidity (IN): "));
  Serial.print(h);
  Serial.print(F("%,  Temperature (IN): "));
  Serial.print(t);
  Serial.print(F("C , "));
  Serial.print(F("Humidity (OUT): "));
  Serial.print(h_out);
  Serial.print(F("%,  Temperature (OUT): "));
  Serial.print(t_out);
 Serial.print(F("C, "));
  Serial.println(count);

}
