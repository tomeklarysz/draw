#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ESP32Encoder.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
ESP32Encoder encX;
ESP32Encoder encY;

int posX = 64;
int posY = 32;

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
  int newX = encX.getCount();
  int newY = encY.getCount();

  // krawedzie
  if (newX < 0) { newX = 0; encX.setCount(0); }
  if (newX > 127) { newX = 127; encX.setCount(127); }
  if (newY < 0) { newY = 0; encY.setCount(0); }
  if (newY > 63) { newY = 63; encY.setCount(63); }

  if (newX != posX || newY != posY) {
    u8g2.drawPixel(newX, newY); 
    u8g2.sendBuffer(); // Wysyłamy aktualizację na ekran
    
    posX = newX;
    posY = newY;
  }
}