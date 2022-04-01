/*
ToDo
- add poti zum temp sollwert einstellen
- add anzeige ob Heizung an oder aus ist
- add writing to log file (append mode, brauche ich das?)
  - overwrite if file exists..?
*/

#include "DHT.h"           // for temperature sensor
#include "LiquidCrystal.h" // for LCD display
#include <SPI.h>           // for SD card
#include <SD.h>            // for SD card
#include <RTClib.h>        // for getting the current time

// ######################### set I/O's ##############################################
#define DHTPIN 2            // Digital pin connected to the DHT sensor (inside temp)
#define DHTPIN8 8           // Digital pin connected to the DHT sensor (outside temp)

const int relaisIN2 = 3;    // pin for relais

const int TempSetUp = 11;   // button temperature setpoint up
const int TempSetDown = 12; // button temperature setpoint down

// pins for LCD Display
const int rs = 10;
const int en = 9;
const int d4 = 22; 
const int d5 = 24; 
const int d6 = 26; 
const int d7 = 28;

// ############################### constants ############################################
int   count = 0;                 // counter to switch LCD display between inside and outside temp
float TempSetDay    = 20.0;      // temperature setpoint day
float TempSetNight  = 12.0;      // temperature setpoint day
float TempHyst = 2.0;            // hysteresis for temp control

int LoggerCount = 0;             // count to make logging to SD card less often 
const int LoggerThrsh = 150;     // ToDo: make this relavive to the 2s time the dispaly is refreshed
                                 //       we want the SD card to be updated only every 5min
char filename[] = "TempLog.csv"; // only 8 characters are allowed

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

float PowerHeater = 2.0;         // electrical power of connected heater [kW]

// ############################### variables ############################################
float TempSet;                   // actual temperature setpoint
bool HeatingOn = false;          // boolean to show if heating is on or off
float t_inSum;
float t_outSum;
float h_inSum;
float h_outSum;

int heatOnTime;

float EnergyCulmulative;         // culmulative energie consumed by heater [kWh]

// ######################### create classes #################################
#define DHTTYPE DHT22            // DHT 22 (the DHT11 is the low presission one)

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);        // create Temp sensor (DHT)  
DHT dht2(DHTPIN8, DHTTYPE);      // create Temp sensor (DHT)

LiquidCrystal lcd(rs, en, d4, d5, d6, d7);  // create LCD display

RTC_DS3231  rtc;                 // create RTC clock

// #########################################################################
// ########################     i n i t     ################################
// #########################################################################
void setup() {
  Serial.begin(9600);

  // RTC (real time clock) init
  if (!rtc.begin()) 
  {
    Serial.println("Couldn't find RTC");
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // sets to system time of computer (time of upload)

  // Temp sensor init
  dht.begin();  
  dht2.begin();  

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("starting...");

  // set relais output to test relais
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
  
  // write header to SD file
  File dataFile = SD.open(filename, FILE_WRITE);
  if (dataFile) 
  {
	auto dateTimeString = getDateTimeString();

    dataFile.println("=================================================================");
    dataFile.println("         Temperature Datalogger by Martin Staedler (2022) ");
  	dataFile.println("         Start time of logging: " + dateTimeString);
    dataFile.println("=================================================================");
    dataFile.println("");
    dataFile.println("Date; Time; In Temp [deg]; In Humitity [%]; Out Temp [deg]; Out Humitity [%]; Temp Set [deg]; heater On [s]; Energy [kWh] ");
    dataFile.close();
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
  // Wait a 2s seconds between measurements.
  delay(2000);
  DateTime now = rtc.now();

  // ####################### Temp mesaurement #####################
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) 
  {
    Serial.println(F("Failed to read from DHT sensor (inside)!"));
    return;
  }

  float h_out = dht2.readHumidity();
  // Read temperature as Celsius (the default)
  float t_out = dht2.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(h_out) || isnan(t_out)) 
  {
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
         // ToDo: setpoint anzeigen solange ther taster gedrückt ist, oder lange drücken um in den set mode zu kommen?
        }
      }
    }

  // ToDo: das ganze für temp down, Function bauen?

  // ####################### Temp Control ######################
  if ((now.hour() > 8) && (now.hour() < 20))
  {
    TempSet = TempSetDay; // night temp setpoint
  }
  else
  {
    TempSet = TempSetNight; // night temp setpoint
  }


  // switch ON/OFF heating
  if (t > (TempSet + TempHyst))
  {
    // heating OFF
    HeatingOn = false;
    digitalWrite(relaisIN2, LOW);  // Relais2 OFF
  }
  if (t < TempSet)
  {
    // heating ON
	  HeatingOn = true;
  	EnergyCulmulative = EnergyCulmulative + PowerHeater * 2 / 3600; // cycle time is 2s, 3600 to make it kWh
    digitalWrite(relaisIN2, HIGH);  // Relais2 ON
  }

  // ####################### Display #####################
  count++;
  if (count <= 2)
  {
    // print temperature to LCD display
    lcd.setCursor(0, 0);
    lcd.print("                "); //Print blanks to clear the row
    lcd.setCursor(0, 0);
    lcd.print("IN  Temp:");
    lcd.print(t);
    lcd.print("\337C "); // grad Celsuis

    lcd.setCursor(0, 1);
    lcd.print("                "); //Print blanks to clear the row
    lcd.setCursor(0, 1);
    lcd.print("OUT Temp:");
    lcd.print(t_out);
    lcd.print("\337C "); // grad Celsuis
  }
  if (count > 2)
  {
	// print humidity to LCD display
    lcd.setCursor(0, 0);
    lcd.print("                "); //Print blanks to clear the row
    lcd.setCursor(0, 0);
    lcd.print("IN  Feucht:");
    lcd.print(h);

    lcd.setCursor(0, 1);
    lcd.print("                "); //Print blanks to clear the row
    lcd.setCursor(0, 1);
    lcd.print("OUT Feucht:");
    lcd.print(h_out);
    if (count >= 4)
      count = 0;
  }

  // ##################### Looging to SD card ####################
  // prepare averaging of values
  t_inSum  = t_inSum  + t;
  t_outSum = t_outSum + t_out;
  h_inSum  = h_inSum  + h;
  h_outSum = h_outSum + h_out;

  if (HeatingOn)
  {
    heatOnTime = heatOnTime + 2;
  }

  LoggerCount++;
  if (LoggerCount > LoggerThrsh)
  {
	  float t_inAv  = t_inSum  / LoggerCount;
	  float t_outAv = t_outSum / LoggerCount;
	  float h_inAv  = h_inSum  / LoggerCount;
	  float h_outAv = h_outSum / LoggerCount;

    File dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile) 
    {
	    auto dateTimeString = getDateTimeString();
	    dataFile.print(dateTimeString + "; ");

      dataFile.print(t_inAv);
      dataFile.print("; ");
      dataFile.print(h_inAv);
      dataFile.print("; ");
      dataFile.print(t_outAv);
      dataFile.print("; ");
      dataFile.print(h_outAv);
	    dataFile.print("; ");
      dataFile.print(TempSet);
	    dataFile.print("; ");
	    dataFile.print(heatOnTime);
	    dataFile.print("; ");
	    dataFile.println(EnergyCulmulative);	  
      dataFile.close();
    }
    else
    {
      Serial.println("error opening datalog file");
    }
	  t_inSum = 0;
	  t_outSum = 0;
	  h_inSum = 0;
	  h_outSum = 0;
	  heatOnTime = 0;
    LoggerCount = 0;
  }

  // #################### debugging ##############################
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
  Serial.print(heatOnTime);
  Serial.print(F(", "));
  Serial.println(EnergyCulmulative);
}

// #########################################################################
// #################    f u n c t i o n s     ##############################
// #########################################################################
String getDateTimeString()
{
	DateTime now = rtc.now();
	char buf[50];
	sprintf(buf, "%02d-%02d-%4d; %02d:%02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());
	return buf;
}
