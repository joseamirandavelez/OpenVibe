/* ============================================
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
===============================================

OpenVibe 1.0 by José Miranda

An open source wireless vibration data logger device.

This is de Arduino sketch for the OpenVibe wireless vibration data logger device.
OpenVibe is an easy to build, wireless vibration data logger based on inexpensive
hardware. It is designed to be small, but versatile at the same time. It is based
on the ESP8266 microprocessor (Wemos D1 Mini package), a Wemos D1 Mini SD card
shield, and an MPU6050 accelerometer/gyroscope. The device can be build as cheap
as you want, but this was my bill of materials:

BOM:
Wemos D1 Mini clone                    (https://www.amazon.com/dp/B01N3P763C): $9.99 (as of 08/06/17)
Micro SD Card Shield for Wemos D1 Mini (https://www.amazon.com/dp/B071WJ99N8): $7.99 (as of 08/06/17)
MPU6050 3 Axis Accelerometer/Gyroscope (https://www.amazon.com/dp/B01DK83ZYQ): $5.09 (as of 08/06/17)
ProtoShield for Wemos D1 Mini          (https://www.amazon.com/dp/B071ZY9W71): $6.99 (as of 08/06/17)

The total was around $30 USD, which is not bad but could be lower if you go cheaper. I am sure this
can be built for less than $10!

Changelog:
08/06/17: Initial release (v1.0)
Notes:
-In order to be able to mount the MPU6050 sensor directly to the ESP8266, I used
 pins 0 and 2 (D3 and D4) as SDA SCL. 

*/

#include <ESP8266WiFi.h>          // https://github.com/esp8266/Arduino
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <WiFiUDP.h>
#include <NTPClient.h>            // https://github.com/arduino-libraries/NTPClient
#include <I2Cdev.h>               // https://github.com/jrowberg/i2cdevlib
#include <MPU6050.h>
#include <Ticker.h>
#include <SPI.h>
#include <SD.h>

const String versionNumber = "1.0";
boolean recording = false;
String filename = "";
String prefix = "OV";
String message = "";
String messageType;

File dataFile;

std::unique_ptr<ESP8266WebServer> server;

// For LED Status
Ticker ticker;

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for InvenSense evaluation board)
// AD0 high = 0x69
MPU6050 accelgyro;
//MPU6050 accelgyro(0x69); // <-- use for AD0 high

int16_t ax, ay, az;
int16_t gx, gy, gz;

#define LED_PIN 13
bool blinkState = false;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,"time.nist.gov",-14400,100);

void tick()
{
  //toggle state
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void handleRoot() {
  /*
  String message = "Number of args received: ";
  message += server->args();            //Get number of parameters
  message += "\n";                            //Add a new line
  
  for (int i = 0; i < server->args(); i++) {  
    message += "Arg " + (String)i + " > ";   //Include the current iteration value
    message += server->argName(i) + ": ";    //Get the name of the parameter
    message += server->arg(i) + "\n";        //Get the value of the parameter
  }
  Serial.println(message);
  */
  message="";
  if (server->hasArg("start") && server->hasArg("prefix")) {
    String daTime=getTime();
    filename = server->arg("prefix") + daTime + ".csv";
    dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile) {
      Serial.println("Starting Data Log on: " + filename);
      recording = true;
      message = "Started Data Log on" + filename;
      messageType = "alert-info";
      dataFile.println("AX,AY,AZ");
      //Serial.println("AX,AY,AZ");
      ticker.attach(0.2, tick);
    } else {
      Serial.println ("Warning: Could not open '" + filename + "'.");
      message = "Warning: Could not open '" + filename + "'.";
      messageType = "alert-warning";
      recording = false;
      ticker.detach();
    }
  } else if (server->hasArg("stop")) {
    recording = false;
    dataFile.close();
    message = "Logging stopped.";
    messageType = "alert-success";
    Serial.println("Stopped Data Log on: " + filename);
  }
  server->send ( 200, "text/html", getPage() );
}

String getPage(){
  String page = "<html lang='en'><head><title>OpenVibe</title>";
  page += "<link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css'><script src='https://ajax.googleapis.com/ajax/libs/jquery/3.1.1/jquery.min.js'></script><script src='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js'></script>";
  page += "<title><a href='https://github.com/joseamirandavelez/OpenVibe'>OpenVibe v1.0</a></title></head><body style='margin-left:10pt'>";
  page += "<div class='container-fluid'>";
  page +=   "<div class='row'>";
  page +=     "<div class='col-md-12'>";
  page +=       "<h1>OpenVibe " + versionNumber + "</h1>";
  page +=       "<div class='row'>";
  page +=         "<ul class='nav nav-pills'>";
  page +=          "<li class='active'>";
  page +=             "<a href='#'> <span class='badge pull-right'>";
  page +=             "</span> Data</a>";
  page +=           "</li><li>";
  page +=             "<a href='#'> <span class='badge pull-right'>";
  page +=             "</span> Settings</a>";
  page +=           "</li><li>";
  page +=             "<a href='#'> <span class='badge pull-right'>";
  page +=             "</span> Help</a></li>";
  page +=         "</ul>";
  page +=       "</div>";
  page +=       "<h3>Logging</h3>";
  page +=       "<form action='/' method='POST'>";
  page +=         "<div class='row'><div class='col-sm-2'><label for='filename'>Filename prefix:</label><input type='text' class='form-control' name='prefix' value='" + prefix + "' maxlength='2'></div></div>";
  page +=         "<div class='row'><div class='col-sm-2'>Prefix must be no more than 2 letters.</div></div>";
  page +=         "<div class='row'><div class='col-sm-2'>";
  if (!recording) {
    page +=         "<button type='button submit' name='start' value='1' class='btn btn-success btn-lg'>Start</button>";     
  } else {
    page +=         "<button type='button submit' name='stop' value='1' class='btn btn-success btn-lg'>Stop</button>";     
  }
  page +=         "</div></div>";
  page +=       "</form>";
  page +=     "</div>";
  page +=   "</div>";
  page += "</div>";
  if (message.length() > 0) {
    page += "<div class='row'><div class='col-md-4'><div class='alert " + messageType + " fade in'>";
    page +=   message;
    page += "</div></div></div>";
  }
  
  page +=   "<footer class='footer'><div class='container'>Source: <a href='https://github.com/joseamirandavelez/OpenVibe'>https://github.com/joseamirandavelez/OpenVibe</div></footer>";
  page +=  "</div></div></div>";
  page += "</body></html>";
  return page;
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";
  for (uint8_t i = 0; i < server->args(); i++) {
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  server->send(404, "text/plain", message);
}

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void setup() {
  // Using pin 0 (D3) for SDA, and pin 2 (D4) for SCL on Wemos D1 Mini for simplicity of connections wit MPU6050 module
  Wire.begin(0,2);
  Serial.begin(9600);

  Serial.println ("\n\nOpenVibe v" + versionNumber);

  Serial.print("\nInitializing Accelerometer...  ");
  accelgyro.initialize();
  Serial.println(accelgyro.testConnection() ? "Done!" : "Failed");

  // Set Accelerometer offsets. Use IMU_Zero.ini sketch in the i2cdevlib library examples folder to get these values.
  accelgyro.setXAccelOffset(-3750);
  accelgyro.setYAccelOffset(-5900);
  accelgyro.setZAccelOffset(913);
  accelgyro.setXGyroOffset(-32);
  accelgyro.setYGyroOffset(-50);
  accelgyro.setZGyroOffset(-4);

  Serial.print("Initializing SD card...  ");
  // see if the card is present and can be initialized:
  if (!SD.begin(4)) {
    Serial.println("Card initialization failed. Check if the card is inserted. Otherwise, check if it is formatted.");
    // don't do anything more:
    return;
  }
  Serial.println("Done!");


  //WiFiManager
  Serial.print("Initializing Wifi...  ");
  //set led pin as output
  pinMode(LED_PIN, OUTPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  //wifiManager.resetSettings();    //reset saved setting
  wifiManager.setAPCallback(configModeCallback); 
  wifiManager.setSTAStaticIPConfig(IPAddress(192, 168, 0, 100), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0));

  //check if wifi settings saved in EEPROM work. Create Access Point otherwise.
  String ssidString = "OpenVibe_" + String(ESP.getChipId());
  char ssid[ssidString.length()];
  ssidString.toCharArray(ssid,ssidString.length());
  if(!wifiManager.autoConnect(ssid)) {
     Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  Serial.println("Done!");
  ticker.detach();
  digitalWrite(BUILTIN_LED, LOW);   //keep LED on

  //initialize web server
  Serial.print("Initializing HTTP server...  ");
  server.reset(new ESP8266WebServer(WiFi.localIP(), 80));
  server->on("/", handleRoot);
  server->on("/inline", []() {
    server->send(200, "text/plain", "this works as well");
  });
  server->onNotFound(handleNotFound);
  server->begin();
  Serial.print("Done! (IP address: ");
  Serial.print(WiFi.localIP());
  Serial.println(")");

  timeClient.begin();
}

String getTime() {
  timeClient.setTimeOffset(-14400);
  timeClient.update();
  unsigned long rawTime = timeClient.getEpochTime();
  unsigned long hours = (rawTime % 86400L) / 3600;
  String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

  unsigned long minutes = (rawTime % 3600) / 60;
  String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

  unsigned long seconds = rawTime % 60;
  String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

  return hoursStr + "" + minuteStr + "" + secondStr;
}

void loop() {
  server->handleClient();
  // read raw accel/gyro measurements from device
  accelgyro.getAcceleration(&ax, &ay, &az);
  if(recording){
    /*
    Serial.print(ax);
    Serial.print(",");
    Serial.print(ay);
    Serial.print(",");
    Serial.println(az);
    */
    dataFile.print(ax);
    dataFile.print(",");
    dataFile.print(ay);
    dataFile.print(",");
    dataFile.println(az);
  }

  //blinkState = !blinkState;
  //digitalWrite(LED_PIN, blinkState);
}
