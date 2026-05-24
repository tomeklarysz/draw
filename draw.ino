#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ESP32Encoder.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include "secrets.h"

const String TERMINAL_ID = "tomek";

// --- PAMIĘĆ HISTORII RYSUNKÓW ---
String historyCanvas[5];
String historySender[5];
int historyCount = 0;       
int currentHistoryIndex = 0; 

bool isBrowsingHistory = false;
unsigned long historyBarTimer = 0;

int lastDrawX = -1;
int lastDrawY = -1;

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
ESP32Encoder encX;
ESP32Encoder encY;
uint8_t canvas[128][64 / 8];
bool isPenDown = false;
bool isMenuOpen = false;
int menuSelection = 0; 
int lastEncYCount = 0;

int posX = 64;
int posY = 32;

const int SDA_ = 25;
const int SCK_ = 33;

const int ENCX_S1 = 34;
const int ENCX_S2 = 35;
const int ENCX_KEY = 32;

const int ENCY_S1 = 26;
const int ENCY_S2 = 27;
const int ENCY_KEY = 13;

String packCanvas() {
  String output = "";
  for (int i = 0; i < 128; i++) {
    for (int j = 0; j < 8; j++) {
      if (canvas[i][j] < 16) output += "0";
      output += String(canvas[i][j], HEX);
    }
  }
  return output;
}

void unpackCanvas(String input) {
  int charIndex = 0;
  for (int i = 0; i < 128; i++) {
    for (int j = 0; j < 8; j++) {
      if (charIndex + 2 <= input.length()) {
        String hexByte = input.substring(charIndex, charIndex + 2);
        canvas[i][j] = (uint8_t)strtol(hexByte.c_str(), NULL, 16);
        charIndex += 2;
      }
    }
  }
}

void downloadHistory() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(20, 36, "Loading history...");
  u8g2.sendBuffer();

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); 
    HTTPClient http;

    String url = String(FIREBASE_HOST) + "/znikopis_history.json?auth=" + String(FIREBASE_AUTH);
    
    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String response = http.getString();
      if (response != "null") {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response);

        if (!error) {
          JsonObject root = doc.as<JsonObject>();
          int totalIncoming = root.size();
          if (totalIncoming > 5) totalIncoming = 5;
          historyCount = totalIncoming;
          
          int index = totalIncoming - 1; 
          for (JsonPair p : root) {
            JsonObject drawingObj = p.value().as<JsonObject>();
            if (drawingObj.containsKey("sender") && drawingObj.containsKey("canvas_data")) {
              if (index >= 0) {
                historySender[index] = drawingObj["sender"].as<String>();
                historyCanvas[index] = drawingObj["canvas_data"].as<String>();
                index--;
              }
            }
          }
          if (historyCount > 0) currentHistoryIndex = 0; 
        }
      }
    }
    http.end();
  }
}

void uploadDrawing() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(20, 36, "Sending...");
  u8g2.sendBuffer();

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String url = String(FIREBASE_HOST) + "/znikopis_history.json?auth=" + String(FIREBASE_AUTH);
    String jsonPayload = "{\"sender\":\"" + TERMINAL_ID + "\",\"canvas_data\":\"" + packCanvas() + "\",\"timestamp\":{\".sv\":\"timestamp\"}}";

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(jsonPayload);

    if (httpCode == HTTP_CODE_OK || httpCode == 201) {
      downloadHistory();
    }
    http.end();
  }
}

// --- NOWA FUNKCJA: Konfiguracja managera OTA ---
void setupOTA() {
  ArduinoOTA.setHostname("znikopis-tomek-v2");
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(10, 30, "OTA Update...");
    u8g2.sendBuffer();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int percent = progress / (total / 100);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(10, 25, "Updating code:");
    u8g2.drawFrame(10, 35, 108, 10);
    u8g2.drawBox(12, 37, (percent * 104) / 100, 6);
    u8g2.sendBuffer();
  });

  ArduinoOTA.onEnd([]() {
    u8g2.clearBuffer();
    u8g2.drawStr(10, 36, "Success! Restart...");
    u8g2.sendBuffer();
  });

  ArduinoOTA.begin();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_, SCK_);
  u8g2.begin();

  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encX.attachHalfQuad(ENCX_S2, ENCX_S1);
  encX.setCount(posX);
  encY.attachHalfQuad(ENCY_S1, ENCY_S2);
  encY.setCount(posY);

  pinMode(ENCX_KEY, INPUT_PULLUP);
  pinMode(ENCY_KEY, INPUT_PULLUP);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 25, "Connect to WiFi:");
  u8g2.drawStr(10, 40, "'Terminal-Setup'");
  u8g2.sendBuffer();

  WiFiManager wm;
  wm.autoConnect("Terminal-Setup");
  Serial.println("Połączono z WiFi!");

  // Uruchomienie obsługi bezprzewodowej
  setupOTA();

  downloadHistory();
}

void loop() {
  // --- KLUCZOWE: Sprawdzanie żądań OTA w każdej klatce ---
  ArduinoOTA.handle();

  int x = posX, y = posY;
  
  if (isBrowsingHistory) {
    int currentEncYCount = encY.getCount();
    if (currentEncYCount != lastEncYCount) {
      if (currentEncYCount > lastEncYCount) currentHistoryIndex++; 
      else currentHistoryIndex--;

      if (currentHistoryIndex >= historyCount) currentHistoryIndex = 0;
      if (currentHistoryIndex < 0) currentHistoryIndex = historyCount - 1;

      unpackCanvas(historyCanvas[currentHistoryIndex]); 
      lastEncYCount = currentEncYCount;
      historyBarTimer = millis();
    }
  } 
  else if (!isMenuOpen) {
    x = encX.getCount() % 128;
    if (x < 0) { x = 0; encX.setCount(0); }
    if (x > 127) { x = 127; encX.setCount(127); }

    y = encY.getCount() % 64;
    if (y < 0) { y = 0; encY.setCount(0); }          
    if (y > 63) { y = 63; encY.setCount(63); }      
  } else {
    int currentEncYCount = encY.getCount();
    if (currentEncYCount != lastEncYCount) {
      if (currentEncYCount > lastEncYCount) menuSelection++; 
      else menuSelection--; 
      
      if (menuSelection > 2) menuSelection = 0;
      if (menuSelection < 0) menuSelection = 2;
      
      lastEncYCount = currentEncYCount; 
    }
  }

  if (digitalRead(ENCX_KEY) == LOW) {
    isPenDown = !isPenDown;
    if (isPenDown) { lastDrawX = -1; lastDrawY = -1; }
    delay(200);
  }

  if (digitalRead(ENCY_KEY) == LOW) {
    delay(200);
    if (isBrowsingHistory) {
      isBrowsingHistory = false;
      memset(canvas, 0, sizeof(canvas));
    } else if (!isMenuOpen) {
      isMenuOpen = true;
      menuSelection = 0;
      lastEncYCount = encY.getCount();
    } else {
      if (menuSelection == 0) {
        uploadDrawing();
      } else if (menuSelection == 1) {
        downloadHistory(); 
        if (historyCount > 0) {
          isBrowsingHistory = true;
          unpackCanvas(historyCanvas[currentHistoryIndex]);
          lastEncYCount = encY.getCount(); 
          historyBarTimer = millis();
        }
      } else if (menuSelection == 2) {
        memset(canvas, 0, sizeof(canvas));
        isPenDown = false;
        x = posX; encX.setCount(posX);
        y = posY; encY.setCount(posY);
      }
      isMenuOpen = false; 
    }
  }

  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  for (int i = 0; i < 128; i++) {
    for (int j = 0; j < 64; j++) {
      if (canvas[i][j/8] & (1 << (j%8))) {
        u8g2.drawPixel(i, j);
      }
    }
  }

  bool blinkState = (millis() / 250) % 2;
  
  if (isBrowsingHistory) {
    if (millis() - historyBarTimer < 3000) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(0, 0, 128, 11); 
      u8g2.setDrawColor(1);
      u8g2.setFont(u8g2_font_5x7_tf);
      String topBar = "[" + historySender[currentHistoryIndex] + "] " + String(currentHistoryIndex + 1) + "/" + String(historyCount);
      u8g2.drawStr(4, 8, topBar.c_str());
      u8g2.drawFrame(0, 11, 128, 1);
    }
  } else if (!isMenuOpen) {
    if (isPenDown) {
      if (lastDrawX == -1 && lastDrawY == -1) {
        lastDrawX = x; lastDrawY = y;
      } 
      else if (x != lastDrawX || y != lastDrawY) {
        canvas[x][y/8] |= (1 << (y%8));
        lastDrawX = x; lastDrawY = y;
      }
      if (blinkState) { u8g2.setDrawColor(1); u8g2.drawPixel(x, y); } 
      else { u8g2.setDrawColor(2); u8g2.drawPixel(x, y); }
    } else {
      if (blinkState) { u8g2.setDrawColor(2); u8g2.drawFrame(x - 2, y - 2, 5, 5); }
    }
  } else {
    u8g2.setDrawColor(0); u8g2.drawBox(30, 10, 68, 44);
    u8g2.setDrawColor(1); u8g2.drawFrame(30, 10, 68, 44);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(34, 22, menuSelection == 0 ? "> Send" : "  Send");
    u8g2.drawStr(34, 34, menuSelection == 1 ? "> History" : "  History");
    u8g2.drawStr(34, 46, menuSelection == 2 ? "> Clear" : "  Clear");
  }

  u8g2.sendBuffer();
}