#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ESP32Encoder.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
ESP32Encoder encX;
ESP32Encoder encY;
uint8_t canvas[128][64 / 8];
bool isPenDown = false;
bool isMenuOpen = false;
int menuSelection = 0; // 0 - send, 1 - clear
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
}

void loop() {

  int x = posX, y = posY;
  if (!isMenuOpen) {
    x = encX.getCount() % 128;
    if (x < 0) { x = 0; encX.setCount(0); }
    if (x > 127) { x = 127; encX.setCount(127); }

    y = encY.getCount() % 64;
    if (y < 0) { x = 0; encY.setCount(0); }
    if (y > 63) { x = 127; encY.setCount(63); }
  } else {
    int currentEncYCount = encY.getCount();
    
    // Sprawdzamy, czy nastąpił ruch gałką
    if (currentEncYCount != lastEncYCount) {
      if (currentEncYCount > lastEncYCount) {
        menuSelection++; // Ruch w prawo/dół
      } else {
        menuSelection--; // Ruch w lewo/góra
      }
      
      // Ograniczamy menuSelection do przedziału 0-1
      if (menuSelection > 1) menuSelection = 0;
      if (menuSelection < 0) menuSelection = 1;
      
      lastEncYCount = currentEncYCount; // Zapamiętujemy pozycję
    }
  }

  if (digitalRead(ENCX_KEY) == LOW) {
    isPenDown = !isPenDown;
    delay(200);
  }

  if (digitalRead(ENCY_KEY) == LOW) {
    delay(200);
    if (!isMenuOpen) {
      isMenuOpen = true;
    } else {
      if (menuSelection == 0) {
        // Tu będzie funkcja wysyłania: sendCanvasOverWiFi();
      } else if (menuSelection == 1) {
        // Czyszczenie canvasu
        memset(canvas, 0, sizeof(canvas));
        isPenDown = false;
        x = posX;
        encX.setCount(posX);
        y = posY;
        encY.setCount(posY);
      }
      isMenuOpen = false; // Zamykamy menu po akcji
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

  if (!isMenuOpen) {
    
    // jesli pen down to chcemy rysowac to co w canvas
    // jesli nie to chcemy tryb xor
    if (isPenDown) {
      canvas[x][y/8] |= (1 << (y%8));

      if (blinkState) {
        u8g2.setDrawColor(1);
        u8g2.drawPixel(x, y);
      } else {
        u8g2.setDrawColor(2);
        u8g2.drawPixel(x, y);
      }
    } else {
      if (blinkState) {
        u8g2.setDrawColor(2); 
        u8g2.drawFrame(x - 2, y - 2, 5, 5);
      }
    }
  } else {
    u8g2.setDrawColor(0); // Czarny prostokąt, żeby zakryć rysunek pod spodem
    u8g2.drawBox(34, 16, 60, 32);
    
    u8g2.setDrawColor(1); // Biała ramka i tekst
    u8g2.drawFrame(34, 16, 60, 32);
    u8g2.setFont(u8g2_font_6x10_tf);
    
    // Wskaźnik wyboru (strzałka ">")
    u8g2.drawStr(38, 28, menuSelection == 0 ? "> Send" : "  Send");
    u8g2.drawStr(38, 40, menuSelection == 1 ? "> Clear" : "  Clear");
  }

  u8g2.sendBuffer();

}