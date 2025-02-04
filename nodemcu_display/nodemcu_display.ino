#include <Wire.h>
#include "SSD1306Wire.h" // and ESP32 OLED Driver for SSD1306 displays by ThingPulse: https://github.com/ThingPulse/esp8266-oled-ssd1306

//Pinos do NodeMCU
// SDA => D5
// SCL => D6
// Inicializa o display Oled
SSD1306Wire  display(0x3c, D5, D6);

void setup() {
  Serial.begin(115200);

  // Init display
  display.init();
  display.flipScreenVertically();



}

void loop() {
  // put your main code here, to run repeatedly:

}
