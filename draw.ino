#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ESP32Encoder.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
ESP32Encoder encX;
ESP32Encoder encY;
uint8_t canvas[128][64 / 8];
bool isPenDown = false;

int posX = 64;
int posY = 32;

const int LEFT_ENC_KEY_PIN = 15;

void setup() {
  Wire.begin(26, 27);
  u8g2.begin();

  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encX.attachHalfQuad(22, 23);
  encX.setCount(posX);

  encY.attachHalfQuad(34, 35);
  encY.setCount(posY);
}

void loop() {
  int x = encX.getCount() % 128;
  int y = encY.getCount() % 64;

  if (digitalRead(LEFT_ENC_KEY_PIN) == LOW) {
    isPenDown = !isPenDown;
    delay(200);
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

  // jesli pen down to chcemy rysowac to co w canvas
  // jesli nie to chcemy tryb xor
  if (isPenDown) {
    canvas[x][y/8] |= (1 << (y%8));
    u8g2.drawPixel(x, y);
  } else {
    u8g2.setDrawColor(2); 
    u8g2.drawFrame(x - 2, y - 2, 5, 5);
  }

  u8g2.sendBuffer();

}