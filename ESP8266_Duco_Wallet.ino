/*
/////////////////////////////////////////////////////////////////////////////////
//  _____                            _         _       _    _            _     //
// |  __ \                          | |   _   | | ____| |  | |    ____ _| |_   //
// | |  | |_   _  __   ___          | |  | |  | |(__  | |  | |   |    |_   _|  //
// | |  | | | | |/ _| / _ \   ____  | |  | |  | | __| | |  | |   |  __| | |    //
// | |__| | |_| | (_ | |_| | |____| |  \/   \/  |/ ,, | |_ | |__ | (__  | |_   //
// |_____/ \__,_|\__| \___/          \____/\___/ \___/ \__) \___) \___/  \__|  //
/////////////////////////////////////////////////////////////////////////////////
//  Code for ESP8266 boards - V2.55                                            //
//  © Duino-Coin Community 2019-2021                                           //
//  Distributed under MIT License                                              //
/////////////////////////////////////////////////////////////////////////////////
//  https://github.com/revoxhere/duino-coin - GitHub                           //
//  https://duinocoin.com - Official Website                                   //
//  https://discord.gg/k48Ht5y - Discord                                       //
//  https://github.com/revoxhere - @revox                                      //
//  https://github.com/JoyBed - @JoyBed                                        //
//  https://github.com/kickshawprogrammer - @kickshawprogrammer                //
/////////////////////////////////////////////////////////////////////////////////
//Rewrite by https://github.com/sangrapuan-toraya                              //
/////////////////////////////////////////////////////////////////////////////////
*/

#include <ESP8266WiFi.h> // Include WiFi library
#include <ESP8266mDNS.h> // OTA libraries
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <EasyScheduler.h>
#define DUINOCOIN_OLED
#include "Adafruit_ILI9341.h"
#include <ArduinoJson.h>   //You need to downgrade your ArduinoJSON Library (version 5) to make this code working

#define OLED_SDA 4 // D2 - A4 - GPIO4
#define OLED_SCL 5 // D1 - A5 - GPIO5
#define OLED_ADDR 0x3C

#ifndef DUINOCOIN_OLED
void oled_setup(){};
void oled_display(String s){};
#endif

#ifdef DUINOCOIN_OLED

#include <Wire.h>
#include "SSD1306Wire.h"

#define OLED_RUNEVERY 500
#define OLED_TIMEOUT 4

SSD1306Wire display(OLED_ADDR, OLED_SDA, OLED_SCL);
//SSD1306 display(OLED_ADDR, OLED_SDA, OLED_SCL);// i2c ADDR & SDA, SCL on wemoss

#define StaticJsonDocument<4000> doc;
ESP8266WiFiMulti WiFiMulti;

Schedular MonitorTask; // create Task

//////////////////////////////////////////////////////////
// NOTE: If during compilation, the below line causes a
// "fatal error: Crypto.h: No such file or directory"
// message to occur; it means that you do NOT have the
// latest version of the ESP8266/Arduino Core library.
//
// To install/upgrade it, go to the below link and
// follow the instructions of the readme file:
//
//       https://github.com/esp8266/Arduino
//////////////////////////////////////////////////////////
#include <Crypto.h>  // experimental SHA1 crypto library
using namespace experimental::crypto;

#include <Ticker.h>

const char* SSID           = "YOUR WIFI NAME";   // Change this to your WiFi name
const char* PASSWORD       = "WIFI PASSWORD";    // Change this to your WiFi password
const char* USERNAME       = "DUINO COIN USERNAME";     // Change this to your Duino-Coin username

#define LED_BUILTIN 2
#define BLINK_REFRESH        1
#define BLINK_SETUP_COMPLETE 2
#define BLINK_CLIENT_CONNECT 3
#define BLINK_RESET_DEVICE   5

void SetupWifi() {
  Serial.println("Connecting to: " + String(SSID));
  WiFi.mode(WIFI_STA); // Setup ESP in client mode
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(SSID, PASSWORD);

  int wait_passes = 0;
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++wait_passes >= 10) {
      WiFi.begin(SSID, PASSWORD);
      wait_passes = 0;
    }
  }

  Serial.println("\nConnected to WiFi!");
  Serial.println("Local IP address: " + WiFi.localIP().toString());
}

void blink(uint8_t count, uint8_t pin = LED_BUILTIN) {
  uint8_t state = HIGH;

  for (int x = 0; x < (count << 1); ++x) {
    digitalWrite(pin, state ^= HIGH);
    delay(50);
  }
}

void RestartESP(String msg) {
  Serial.println(msg);
  Serial.println("Resetting ESP...");
  blink(BLINK_RESET_DEVICE);
  ESP.reset();
}

// https://stackoverflow.com/questions/9072320/split-string-into-string-array
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int max_index = data.length() - 1;

  for (int i = 0; i <= max_index && found <= index; i++) {
    if (data.charAt(i) == separator || i == max_index) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == max_index) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void oled_setup() {
  //---------------Set OLED Monitor -------------------------//
  #ifdef OLED_RST
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, HIGH);
  #endif
  display.init();
  display.resetDisplay();
  display.displayOn();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  
  display.setContrast(255);

  display.setI2cAutoInit(true);

  oled_display("- DUCO Wallet v.0.01 -");
  delay(10);
              }
//--------Show DuCo Coin--------------------//


void oled_display(String s) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 0, s);
  display.display();
  delay(10);
}

void Monitor() {
    if ((WiFiMulti.run() == WL_CONNECTED))
  {
    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
    client->setInsecure();
    String serverPath = "https://server.duinocoin.com/users/"+String(USERNAME);
    HTTPClient https;
    https.setTimeout(60000);
    Serial.print("[HTTPS] begin...\n");
    
    if (https.begin(*client, serverPath))  // HTTPS
    {
      int httpCode = https.GET();
      // httpCode will be negative on error
      if (httpCode > 0)
      {
        Serial.print("[HTTPS] GET.....code: Load...\n");
      } else {
      Serial.printf("[HTTP] ... failed, error: %s\n", https.errorToString(httpCode).c_str());
        oled_display("DuinoCoin.");
    }
      // HTTP header has been send and Server response header has been handled
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        String payload = https.getString();
        DynamicJsonBuffer jsonBuffer(500);
        const char* json = payload.c_str();
        JsonObject& root = jsonBuffer.parseObject(json);
        JsonObject& data = root["result"];
        JsonObject& balance0 = root["result"]["balance"];
        JsonObject& miners0 = root["result"]["miners"]["0"];

        const char* username = balance0["username"];
        float balance = balance0["balance"].as<float>();
        int n = data["miners"].size(); //Total miners
        Serial.println(username);
        Serial.println(balance, 8);
        Serial.println(n); 

           //Total hashrate
        float hashrate =0.000000;
        for(int i=0; i<n;i++)
        {
            float hr = data["miners"][i]["hashrate"];
            hashrate = hashrate + hr;
        }
        Serial.println(hashrate);
        String txt_msg = ""; 
        txt_msg = String(hashrate / 1000, 2);  
        
        oled_display(String("- DUCO Wallet -") + "\n" + "Account: " + String(username) + "\n" + "Balance: " + String(balance) + " Duco." + "\n" + "Miners: " + String(n) + " Worker" + "\n" + "Hashrate: " + String(txt_msg) + " kH/s");
      }
      https.end();
    }
  }
  blink(BLINK_REFRESH);
}

void oled_turnoff() {
  display.clear();
  display.displayOff();
  display.end();
}

static int oled_i;
static String oled_mode_string;
static int oled_mode_timeout;
static String oled_status_string;
static int oled_status_timeout;

boolean oled_loop() {
  if (oled_runEvery(OLED_RUNEVERY)) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    String str = "";
    if (oled_mode_string == "")
    {
      str += timeOn(0);
    }
    else
    {
      str += oled_mode_string;
    }
    display.drawString(64, 0, "Job received, working...");
    display.display();

    if (--oled_status_timeout == 0)
    {
      oled_status_timeout = 0;
      oled_status_string = "";
    }

    if (--oled_mode_timeout == 0)
    {
      oled_mode_timeout = 0;
      oled_mode_string = "";
    }

    return true;
  }
  return false;
}

void oled_status(String status) {
  oled_status_string = status;
  oled_status_timeout = OLED_TIMEOUT;
  Serial.println(String(status));
}

void oled_mode(String status) {
  oled_mode_string = status;
  oled_mode_timeout = OLED_TIMEOUT;
}

boolean oled_runEvery(unsigned long interval)
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}

String timeOn(unsigned long diff)
{
  String str = "";
  unsigned long t = millis() / 1000;
  int s = t % 60;
  int m = (t / 60) % 60;
  int h = (t / 3600);
  str += h;
  str += ":";
  if (m < 10)
    str += "0";
  str += m;
  str += ":";
  if (s < 10)
    str += "0";
  str += s;
  return str;
}

#endif

void setup() {
  // Start serial connection
  Serial.begin(500000);
  Serial.println("\nDUCO WALLET v0.01");

  // Prepare for blink() function
  pinMode(LED_BUILTIN, OUTPUT);
  oled_setup();
  SetupWifi();

  // Sucessfull connection with wifi network
  oled_display("LOADING....");
  delay(5);
  blink(BLINK_SETUP_COMPLETE);

//  chipID = String(ESP.getChipId(), HEX);
  Monitor();
  MonitorTask.start();
}

void loop() {
  //Run monitor every 1 minute
  MonitorTask.check(Monitor, 60000); //1000 = 1 second
}
