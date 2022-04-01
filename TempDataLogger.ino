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
bool  showTemp;                  // if true temperature will be displayed
float TempSetDay    = 20.0;      // temperature setpoint day
float TempSetNight  = 12.0;      // temperature setpoint day
float TempHyst = 2.0;            // hysteresis for temp control

int LoggerCount = 0;             // count to make logging to SD card less often 
const int LoggerThrsh = 6;       // ToDo: make this relavive to the 10s time the dispaly is refreshed
                                 //       we want the SD card to be updated only every 1min
char filename[] = "TempLog.csv"; // only 8 characters are allowed

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

float PowerHeater = 2.0;         // electrical power of connected heater [kW]

// ############################### variables ############################################
float TempSet;                   // actual temperature setpoint
bool HeatingOn = false;          // boolean to show if heating is on or off

int heatOnTime;

float EnergyCulmulative;         // culmulative energie consumed by heater [kWh]

bool MeasurementCycle;
bool MeasurementCycle_old;

String Heater = "OFF";

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
  pinMode(53, OUTPUT);
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
    DateTime now = rtc.now();
    auto dateTimeString = createDateTimeString(now);

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
  delay(100);
  DateTime now = rtc.now();
  MeasurementCycle_old = MeasurementCycle;
  MeasurementCycle = (now.second() == 0) || (now.second() == 10) || (now.second() == 20) || (now.second() == 30) || (now.second() == 40) || (now.second() == 50);
  if (MeasurementCycle && MeasurementCycle != MeasurementCycle_old)
  {
    // measure every 10s, and make sure we measure just once in 10s

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
      TempSet = TempSetDay;  // day temp setpoint
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
      Heater = "OFF";
      digitalWrite(relaisIN2, LOW);  // Relais2 OFF
    }
    if (t < TempSet)
    {
      // heating ON
      HeatingOn = true;
      Heater = "ON ";
      heatOnTime = heatOnTime + 10;
      EnergyCulmulative = EnergyCulmulative + PowerHeater * 10 / 3600; // cycle time is 10s, 3600 to make it kWh
      digitalWrite(relaisIN2, HIGH);  // Relais2 ON
    }

    // ####################### Display #####################
    if (showTemp)
    {
      // print Temperature to LCD display
      String TempIN  = "TS:" + String(TempSet,1) + "|TI:" + String(t, 1)  + "\337";
      String TempOUT = "H :" + Heater + " |TO:" + String(t_out, 1) + "\337";
      PrintToLCD(TempIN, TempOUT);
      showTemp = false;
    }
    else
    {
      // print humidity to LCD display
      String HumIN  = "       |HI:" + String(h, 1)  + "%";
      String HumOUT = "H :" + Heater + " |HO:" + String(h_out, 1) + "%";
      PrintToLCD(HumIN, HumOUT);
      showTemp = true;
    }

    // ##################### Looging to SD card ####################
    auto dateTimeString = createDateTimeString(now);
    String OutStr = dateTimeString + "; " + String(t, 1) + "; " + String(h, 1) + "; " + String(t_out, 1) + "; " + String(h_out, 1) +
      "; " + String(TempSet, 1) + "; " + String(heatOnTime) + "; " + String(EnergyCulmulative, 1);
    LoggerCount++;
    if (LoggerCount >= LoggerThrsh)
    {

      File dataFile = SD.open(filename, FILE_WRITE);
      if (dataFile)
      {
        dataFile.println(OutStr);
        dataFile.close();
      }
      else
      {
        Serial.println("error opening datalog file");
      }
      heatOnTime = 0;
      LoggerCount = 0;
    }

    // #################### debugging ##############################
    Serial.println(OutStr);
  }
}

// #########################################################################
// #################    f u n c t i o n s     ##############################
// #########################################################################
String createDateTimeString(DateTime now)
{
	char buf[50];
	sprintf(buf, "%02d-%02d-%4d; %02d:%02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());
	return buf;
}

void PrintToLCD(String line1, String line2)
{
  lcd.setCursor(0, 0);
  lcd.print("                "); //Print blanks to clear the row
  lcd.setCursor(0, 0);
  lcd.print(line1);

  lcd.setCursor(0, 1);
  lcd.print("                "); //Print blanks to clear the row
  lcd.setCursor(0, 1);
  lcd.print(line2);
}



