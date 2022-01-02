// include the library code:
//#include <Adafruit_CharacterOLED.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "time.h"
#include "preferences.h" //file containing User preferences
#include <LiquidCrystal.h>

/*
  Adafruit OLED Wiring Guide (https://www.adafruit.com/product/823):
   OLED pin  <-->   ESP32 pin
    1 (gnd)         GND
    2 (vin)         VIN
    4 (rs)          14
    5 (rw)          32
    6 (en)          26
   11 (d4)          33
   12 (d5)          27
   13 (d6)          12
   14 (d7)          13
*/

#define SWITCH_PIN 25 //sets pin for direction toggle switch connect other end to GND

//This defines the direction variable differently depending on if you are using a toggle switch
#ifndef DIRECTION
char *direction = "N";
#else
const char *direction = DIRECTION;
#endif

//Declaring global variables
unsigned long lastRequestTime = 0;
unsigned long lastDisplayTime = 0;
time_t currentEpochTime;
struct tm currentTime;
char display[20];
char displayList[8][20];
int listCount = 1;
bool forceRefresh = true;
bool switchState = true;
char url[sizeof(serverIP) + sizeof(stationID) + 19];
int numberOfArrivals;

LiquidCrystal *lcd;

void setup()
{
  lcd = new LiquidCrystal(14, 32, 26, 33, 27, 12, 13);

  // Setup
  lcd->begin(16, 2);

  // Connect to the WiFi network
  connectWifi();

  //Configure local time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getLocalTime(&currentTime);
  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print(&currentTime, "%H:%M:%S");
  lcd->setCursor(0, 1);
  lcd->print(&currentTime, "%B %d %Y");
  lcd->clear();
  

  // Make url for MTA data request
  sprintf(url, "http://%s:8080/by-id/%s", serverIP, stationID);

  delay(1000);
}

void loop()
{

  //Send an HTTP GET request every time interval
  if (forceRefresh || (millis() - lastRequestTime) > requestInterval)
  {

    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED)
    {

      //gets current local time in epoch and tm format respectively
      time(&currentEpochTime);
      getLocalTime(&currentTime);

      //Make server request and parse into JSON object
      JSONVar obj = JSON.parse(httpGETRequest(url));



      // JSON.typeof(jsonVar) can be used to get the type of the var
      if (JSON.typeof(obj) == "undefined")
      {
        lcd->setCursor(0,0);
        lcd->print("Parsing input failed!");
        return;
      }

      //Pulls out the relevant data as an JSONVar array
      JSONVar arrivalsArr = obj["data"][direction];
      numberOfArrivals = arrivalsArr.length();

      //Initiate a count of arrivals that will be missed per timeToStation
      int missed = 0;

      //Clear the displayList
      memset(&displayList, 0, sizeof(displayList));

      //Iterates through each pending arrival
      for (int i = 0; i < numberOfArrivals; i++)
      {

        //Extracts the name of the train for the given arrival
        String trainName = JSON.stringify(arrivalsArr[i]["route"]).substring(1,2);

        //Extracts the arrival time of the train in epoch time
        long arrivalTime = convertToEpoch(JSON.stringify(arrivalsArr[i]["time"]));

        //Calculates how many minutes to arrival by comparing arrival time to current time
        int minutesAway = (arrivalTime - currentEpochTime) / 60;

        //Filters out trains that you can't possibly catch
        if (minutesAway >= timeToStation)
        {

          //Constructs the display string
          sprintf(display, "(%s) %s %d Min", trainName, (direction == "N") ? "8 Av." : "BKLYN", minutesAway);
          //Adds the given arrival to the display list for the lcd
          strcpy(displayList[i - missed], display);
        }
        else
        {
          missed++; //increment count of missed trains per timeToStation
        }
      }
      
      //Display the next arriving train on the first line of the lcd
      lcd->setCursor(0, 0);
      lcd->print("                "); //needed to clear the first line
      lcd->setCursor(0, 0);
      lcd->print(displayList[0]);
      direction = "S";
      
      arrivalsArr = obj["data"][direction];
      numberOfArrivals = arrivalsArr.length();

      //Initiate a count of arrivals that will be missed per timeToStation
      missed = 0;

      //Clear the displayList
      memset(&displayList, 0, sizeof(displayList));

      //Iterates through each pending arrival
      for (int i = 0; i < numberOfArrivals; i++)
      {

        //Extracts the name of the train for the given arrival
        String trainName = JSON.stringify(arrivalsArr[i]["route"]).substring(1,2);

        //Extracts the arrival time of the train in epoch time
        long arrivalTime = convertToEpoch(JSON.stringify(arrivalsArr[i]["time"]));

        //Calculates how many minutes to arrival by comparing arrival time to current time
        int minutesAway = (arrivalTime - currentEpochTime) / 60;

        //Filters out trains that you can't possibly catch
        if (minutesAway >= timeToStation)
        {

          //Constructs the display string
          sprintf(display, "(%s) %s %d Min", trainName, (direction == "N") ? "8 Av." : "BKLYN", minutesAway);
          //Adds the given arrival to the display list for the lcd
          strcpy(displayList[i - missed], display);
        }
        else
        {
          missed++; //increment count of missed trains per timeToStation
        }
      }
      
      //Display the next arriving train on the first line of the lcd
      lcd->setCursor(0, 1);
      lcd->print("                "); //needed to clear the first line
      lcd->setCursor(0, 1);
      lcd->print(displayList[0]);
      direction = "N";
    }
    else
    {
      lcd->print("WiFi Disconnected");
      delay(1000);
      connectWifi();
    }
    lastRequestTime = millis();
  }

  //Rotate the arrival displayed on the second line at specified time interval
//  if (forceRefresh || (millis() - lastDisplayTime) > displayInterval)
//  {
//
//    lcd->setCursor(0, 1);
//    lcd->print("                "); //needed to clear the line if the previous display was longer
//    lcd->setCursor(0, 1);
//    lcd->print(displayList[listCount]);
//  delay(3000);
//    listCount++;
//    if (listCount > moreArrivals || listCount >= numberOfArrivals)
//      listCount = 1;
//
//    lastDisplayTime = millis();
//
//    forceRefresh = false;
//  }
forceRefresh = false;
}

void connectWifi()
{

  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print("Joining Wifi");
  lcd->setCursor(0, 1);
  lcd->print(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    lcd->print(".");
    }

  lcd->setCursor(0, 0);
  lcd->print("Personal IP:");
  lcd->setCursor(0, 1);
  lcd->print(WiFi.localIP());
}



String httpGETRequest(char *_url)
{
  HTTPClient http;

//  lcd->setCursor(0, 0);
//  lcd->print("Pinging: ");
//  lcd->setCursor(0, 0);
//  lcd->print(_url);
//  lcd->setCursor(0, 1);
//  lcd->print(&(_url[16]));
  // Connect to server url
  http.begin(_url);

  // Send HTTP GET request
  int httpResponseCode = http.GET();
//  lcd->clear();
//  lcd->setCursor(0, 0);
//  lcd->print("Did get ");

  if (httpResponseCode == 200)
  {
//    lcd->setCursor(0, 0);
//    lcd->print("HTTP SUCCESS ");
    return http.getString();

  }
  else
  {

    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print("HTTP ERROR: ");
    lcd->print(httpResponseCode);
    lcd->setCursor(0, 1);
    lcd->print("SERVER DOWN");
    delay(1000);
  }
}


////Manually parses the timeStamp from the train arrival and returns in epoch time
long convertToEpoch(String timeStamp)
{

  //Uses MTA's timestamp to determine if it's currently daylight savings time
  bool _dst;
  if (timeStamp.substring(20, 23).toInt() != gmtOffset_sec / 3600)
    _dst = 1;
  else
    _dst = 0;

  struct tm t;
  memset(&t, 0, sizeof(tm));                            // Initalize to all 0's
  t.tm_year = timeStamp.substring(1, 5).toInt() - 1900; // This is year-1900, so 112 = 2012
  t.tm_mon = timeStamp.substring(6, 8).toInt() - 1;     //It has -1 because the months are 0-11
  t.tm_mday = timeStamp.substring(9, 11).toInt();
  t.tm_hour = timeStamp.substring(12, 14).toInt();
  t.tm_min = timeStamp.substring(15, 17).toInt();
  t.tm_sec = timeStamp.substring(18, 20).toInt();
  t.tm_isdst = _dst; // Is DST on? 1 = yes, 0 = no, -1 = unknown
  time_t epoch = mktime(&t);

  return epoch;
}
