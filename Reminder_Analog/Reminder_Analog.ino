/*

  Copyright <2018> <Andreas Spiess>

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.


  Based on the HTTPS library of Sujay Phadke ( https://github.com/electronicsguy/ESP8266/tree/master/HTTPSRedirect )

*/

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include "HTTPSRedirect.h"
// #include <credentials.h>

/*
  The credentials.h file at least has to contain:
  char mySSID[]="your SSID";
  char myPASSWORD[]="your Password";
  const char *GScriptIdRead = "............"; //replace with you gscript id for reading the calendar
  const char *GScriptIdWrite = "..........."; //replace with you gscript id for writing the calendar
  It has to be placed in the libraries folder
  If you do not want a credentials file. delete the line: #include <credentials.h>
*/

Ticker blinker;

//Connection Settings
const char* host = "script.google.com";
// const char* googleRedirHost = "script.googleusercontent.com";
const int httpsPort = 443;

unsigned long entryCalender, entryPrintStatus, entrySwitchPressed, heartBeatEntry, heartBeatLedEntry;

unsigned long intEntry;


#define UPDATETIME 20000

#ifdef CREDENTIALS
const char* ssid = mySSID;
const char* password = myPASSWORD;
const char *GScriptIdRead = GoogleScriptIdRead;
const char *GScriptIdWrite = GoogleScriptIdWrite;
#else
//Network credentials
const char* ssid = "........."; //replace with you ssid
const char* password = ".........."; //replace with your password
//Google Script ID
const char *GScriptIdRead = "............"; //replace with you gscript id for reading the calendar
const char *GScriptIdWrite = "..........."; //replace with you gscript id for writing the calendar
#endif

#define NBR_EVENTS 8
String  possibleEvents[NBR_EVENTS] = {"Cat", "Paper", "Cardboard", "Green",  "Laundry", "Fitness", "Meal", "XXX"};
//String  possibleEvents[NBR_EVENTS] = {"Cat", "Paper", "Green", "Cardboard"};
byte  LEDpins[NBR_EVENTS]    = {D0, D1, D2, D4, D5, D6, D7, D8};
bool switchPressed[NBR_EVENTS];
unsigned long switchOff = 0;
boolean beat = false;
int beatLED = 0;

// echo | openssl s_client -connect script.google.com:443 |& openssl x509 -fingerprint -noout
const char* fingerprint = "96 38 33 60 D4 6B 84 C9 32 67 49 44 F2 27 D8 7C 33 1A 35 5A";

enum taskStatus {
  none,
  due,
  done
};

taskStatus taskStatus[NBR_EVENTS];
HTTPSRedirect* client = nullptr;

String calendarData = "";
bool calenderUpToDate;

//Connect to wifi
void connectToWifi() {
  Serial.println();
  Serial.print("Connecting to wifi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Use HTTPSRedirect class to create a new TLS connection
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");

  Serial.print("Connecting to ");
  Serial.println(host);

  // Try to connect for a maximum of 5 times
  bool flag = false;
  for (int i = 0; i < 5; i++) {
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
      flag = true;
      break;
    }
    else
      Serial.println("Connection failed. Retrying...");
  }

  if (!flag) {
    Serial.print("Could not connect to server: ");
    Serial.println(host);
    Serial.println("Exiting...");
    ESP.reset();
  }
  Serial.println("Connected to Google");
}

void printStatus() {
  for (int i = 0; i < NBR_EVENTS; i++) {
    Serial.print("Task ");
    Serial.print(i);
    Serial.print(" Status ");
    Serial.println(taskStatus[i]);
  }
  Serial.println("----------");
}

void getCalendar() {
  //  Serial.println("Start Request");
  // HTTPSRedirect client(httpsPort);
  unsigned long getCalenderEntry = millis();

  // Try to connect for a maximum of 5 times
  bool flag = false;
  for (int i = 0; i < 5; i++) {
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
      flag = true;
      break;
    }
    else
      Serial.println("Connection failed. Retrying...");
  }
  if (!flag) {
    Serial.print("Could not connect to server: ");
    Serial.println(host);
    Serial.println("Exiting...");
    ESP.reset();
  }
  //Fetch Google Calendar events
  String url = String("/macros/s/") + GScriptIdRead + "/exec";
  client->GET(url, host);
  calendarData = client->getResponseBody();
  Serial.print("Calendar Data---> ");
  Serial.println(calendarData);
  calenderUpToDate = true;
  yield();
}

void createEvent(String title) {
  // Serial.println("Start Write Request");

  // Try to connect for a maximum of 5 times
  bool flag = false;
  for (int i = 0; i < 5; i++) {
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
      flag = true;
      break;
    }
    else
      Serial.println("Connection failed. Retrying...");
  }
  if (!flag) {
    Serial.print("Could not connect to server: ");
    Serial.println(host);
    Serial.println("Exiting...");
    ESP.reset();
  }
  // Create event on Google Calendar
  String url = String("/macros/s/") + GScriptIdWrite + "/exec" + "?title=" + title;
  client->GET(url, host);
  //  Serial.println(url);
  Serial.println("Write Event created ");
  calenderUpToDate = false;
}

void manageStatus() {
  for (int i = 0; i < NBR_EVENTS; i++) {
    switch (taskStatus[i]) {
      case none:
        if (switchPressed[i]) {
          digitalWrite(LEDpins[i], HIGH);
          while (!calenderUpToDate) getCalendar();
          if (!eventHere(i)) createEvent(possibleEvents[i]);
          Serial.print(i);
          Serial.println(" 0-->1");
          //getCalendar();
          taskStatus[i] = due;
        } else {
          if (eventHere(i)) {
            digitalWrite(LEDpins[i], HIGH);
            Serial.print(i);
            Serial.println(" 0-->1");
            taskStatus[i] = due;
          }
        }
        break;
      case due:
        if (switchPressed[i]) {
          digitalWrite(LEDpins[i], LOW);
          Serial.print(i);
          Serial.println(" 1-->2");
          taskStatus[i] = done;
        }
        break;
      case done:
        if (calenderUpToDate && !eventHere(i)) {
          digitalWrite(LEDpins[i], LOW);
          Serial.print(i);
          Serial.println(" 2-->0");
          taskStatus[i] = none;
        }
        break;
      default:
        break;
    }
    switchPressed[i] = false;
  }
  yield();
}

bool eventHere(int task) {
  if (calendarData.indexOf(possibleEvents[task], 0) >= 0 ) {
    //    Serial.print("Task found ");
    //    Serial.println(task);
    return true;
  } else {
    //   Serial.print("Task not found ");
    //   Serial.println(task);
    return false;
  }
}

void handleInterrupt() {
  intEntry = millis();
  int reading = analogRead(A0);
  float hi = (reading / (1024.0 / NBR_EVENTS) - 0.5);

  if (hi >= 0) {
    if (switchOff > 10) {
      int button = (int)hi;
      switchPressed[button] = true;
    }
    switchOff = 0;
  }
  else switchOff++;  // no switch pressed
}

void setup() {
  Serial.begin(115200);
  blinker.attach_ms(50, handleInterrupt);

  Serial.println("Reminder_Analog");
  connectToWifi();
  for (int i = 0; i < NBR_EVENTS; i++) {
    pinMode(LEDpins[i], OUTPUT);
    taskStatus[i] = none;  // Reset all LEDs
    switchPressed[i] = false;
  }
  getCalendar();
  entryCalender = millis();
  pinMode(D0, OUTPUT);
}


void loop() {
  if (millis() > entryCalender + UPDATETIME) {
    getCalendar();
    entryCalender = millis();
  }
  manageStatus();
  if (millis() > entryPrintStatus + 5000) {
    printStatus();
    entryPrintStatus = millis();
  }

  if (millis() > heartBeatEntry + 30000) {
    beat = true;
    heartBeatEntry = millis();
  }
  heartBeat();
}

void heartBeat() {
  if (beat) {
    if ( millis() > heartBeatLedEntry + 100) {
      heartBeatLedEntry = millis();
      if (beatLED < NBR_EVENTS) {

        if (beatLED > 0) digitalWrite(LEDpins[beatLED - 1], LOW);
        digitalWrite(LEDpins[beatLED], HIGH);
        beatLED++;
      }
      else {
        for (int i = 0; i < NBR_EVENTS; i++) {
          if (taskStatus[i] == due) digitalWrite(LEDpins[i], HIGH);
          else digitalWrite(LEDpins[i], LOW);
        }
        beatLED = 0;
        beat = false;
      }
    }
  }
}
