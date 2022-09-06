/*
 * Tiny4kOLED - Drivers for SSD1306 controlled dot matrix OLED/PLED 128x32 displays
 *
 * Based on ssd1306xled, re-written and extended by Stephen Denne
 * from 2017-04-25 at https://github.com/datacute/Tiny4kOLED
 *
 * This example shows a full screen rectangle,
 * writes the rectangle size inside the rectangle,
 * and scrolls the size off the screen.
 *
 */

#include <TinyI2CMaster.h>
//#include "TinyWireM.h"
#include <Tiny4kOLED.h>
#include <EEPROM.h>
//#include <avr/pgmspace.h>

struct ITEM {
  String label;
  String extra;
  byte max;
};

const ITEM menu[6] { 
  { "HP", "", 58},
  { "Spells", "Short rest", 2},
  { "Healing", "Long rest", 9 },
  { "Hit Dice", "Long rest (4)", 8 },
  { "Pact Kpr", "Long rest", 1 },
  { "Fire Stf", "1d6+4 at dawn", 10 }
};

byte values[6] = {};

byte MAX_MENU = 5;
byte VRx = A2;
byte VRy = A3;
byte SW = 1;

bool edit_mode = true;
byte selected_val = 0;

// ================
// Debounce control
// ================
int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled

void debounce() {
  int debounceDelay = 50;    // the debounce time; increase if the output flickers

  int reading = digitalRead(SW);                       //A variável leitura recebe a leitura do pino do botão: HIGH (pressionado) ou LOW (Desacionado)

  if (reading != lastButtonState) {                       //Se a leitura atual for diferente da leitura anterior
    lastDebounceTime = millis();                          //Reseta a variável tempoUltimoDebounce atribuindo o tempo atual para uma nova contagem
  }

  if ((millis() - lastDebounceTime) > debounceDelay) { //Se o resultado de (tempo atual - tempoUltimoDebounce) for maior que o tempo que determinamos (tempoDebounce), ou seja, já passou os 50 milissegundos que é o tempo que o botão precisa ficar pressionado para ter a certeza de que ele realmente foi pressionado? Se sim faça:
    if (reading != buttonState) {                         //Verifica se a leitura do botão mudou, ou seja, se é diferente do status que o botão tinha da última vez. Se sim, faça:
      buttonState = reading;                              //statusBotao recebe o que foi lido na variável leitura (pressionado = 1 e não pressionado = 0)
      if (buttonState == HIGH) {                          //Se o statusBotao é igual a HIGH significa que o botão foi pressionado, então faça:
        edit_mode = !edit_mode;
        draw();
      }
    }
  }
  lastButtonState = reading;                            //Atualiza a variável ultimoStatusBotao para o que foi lido na variável leitura

}

void setup() {

  pinMode(VRx, INPUT);
  pinMode(VRy, INPUT);
  pinMode(SW, INPUT_PULLUP);


  
  for (byte i = 0; i < 6; i++) {
    values[i] = EEPROM.read(i);
    if (values[i] < 0 || values[i] > menu[i].max)
      values[i] = menu[i].max;
  }
    

  oled.begin(128, 68, sizeof(tiny4koled_init_128x64r), tiny4koled_init_128x64r);
  oled.enableChargePump();
  oled.setPages(8);
  oled.setFont(FONT6X8);
  oled.clear();
  oled.on();
}

void draw() {
  oled.invertOutput(false);
//  oled.clear();
  byte chrs_valores = 3;
  if (values[edit_mode ? selected_val : 0] >= 100)  chrs_valores += 2;
  else if (values[edit_mode ? selected_val : 0] >= 10)  chrs_valores += 1;
  if (menu[edit_mode ? selected_val : 0].max >= 100)  chrs_valores += 2;
  else if (menu[edit_mode ? selected_val : 0].max >= 10)  chrs_valores += 1;
  
  if (edit_mode && selected_val > 0) {

    // Recupera
    oled.invertOutput(false);
    oled.setCursor(0, 0);
    oled.startData();
    oled.repeatData(0b00000000, 2);
    oled.endData();
    oled.setCursor(2, 0);
    oled.print("Recover");

    // Valores
    oled.startData();
    oled.repeatData(0b00000000, 84 - chrs_valores * 6);
    oled.endData();
    oled.setCursor(128 - chrs_valores * 6, 0);
    oled.print(values[edit_mode ? selected_val : 0]);
    oled.print("/");
    oled.print(menu[edit_mode ? selected_val : 0].max);

    // Extra (Quando recupera)
    oled.setCursor(0, 1);
    oled.startData();
    oled.repeatData(0b00000000, 128);
    oled.endData();
    oled.setCursor(0, 1);
    oled.print(menu[selected_val].extra);
//    oled.startData();
//    oled.repeatData(0b00000000, 128 - 2 - menu[selected_val].extra.length() * 6);
//    oled.endData();

  
  } else {

    // Nome
    oled.invertOutput(edit_mode && selected_val == 0);
    oled.setCursor(0, 0);
    oled.startData();
    oled.repeatData(0b00000000, 2);
    oled.endData();
    oled.setCursor(2, 0);
    oled.print("Teste");

    // Valores
    oled.startData();
    oled.repeatData(0b00000000, 54 - chrs_valores * 6);
    oled.endData();
    oled.setCursor(128 - chrs_valores * 6, 0);
    oled.print(values[edit_mode ? selected_val : 0]);
    oled.print("/");
    oled.print(menu[edit_mode ? selected_val : 0].max);

    
    // HP Bar
    oled.setCursor(0, 1);
    oled.startData();
    oled.sendData(0b00111100);
    oled.sendData(0b01000010);
    oled.repeatData(0b01011010, (values[0] * 124)/ menu[0].max);
    oled.repeatData(0b01000010, 124 - (values[0] * 124)/ menu[0].max);
    oled.sendData(0b01000010);
    oled.sendData(0b00111100);
    oled.endData();
  }

  for (byte im = 1; im < 6; im++) {
    oled.invertOutput(edit_mode && selected_val == im);
    oled.setCursor(0, im + 1);
    oled.startData();
    oled.repeatData(0b00000000, 2);
    oled.endData();

    oled.setCursor(2, im + 1);
    oled.print(menu[im].label);

    oled.startData();
    oled.repeatData(0b00000000, 52 - menu[im].label.length() * 6);
    for (byte b = 0; b < menu[im].max; b++) {
      if (values[im] > b) { 
        oled.sendData(0b00111100);
        oled.sendData(0b01000010);
        oled.repeatData(0b01011010, 4);
        oled.sendData(0b01000010);
      } else {
        oled.sendData(0b00111100);
        oled.repeatData(0b01000010, 6);
      }
    }
    oled.sendData(0b00111100);
    oled.repeatData(0b00000000, 128 - 54 - menu[im].max * 7 - 1);
    oled.endData();
  }
}


void read_analog() {
  if (edit_mode) {
    int max = 685;
    int toler = 15;

    int reading = analogRead(VRx);
    bool up = reading < toler;
    bool down = reading > max - toler;

    reading = analogRead(VRy);
    bool right = reading < toler;
    bool left = reading > max - toler;
    if (up || down) {
      
      if (up && selected_val == 0)
        selected_val = MAX_MENU;
      else if (down && selected_val == MAX_MENU)
        selected_val = 0;
      else
        selected_val += up ? -1 : +1;
      
      draw();
      delay(300);
    }

    if (left or right) {
      if (left && values[selected_val] > 0) {
        values[selected_val] = values[selected_val] - 1;
      }
      if (right && values[selected_val] < menu[selected_val].max) {
        values[selected_val] = values[selected_val] + 1;
      }
      EEPROM.write(selected_val, values[selected_val]);
      draw();

      // HP tem um delay menor que as outras opções
      delay(selected_val == 0 ? 100: 300); 
    }

    if (left or right or up or down)
      lastDebounceTime = millis();
  }
}

void check_idle() {
  int idleDelay = 15000;    // the debounce time; increase if the output flickers
  if (edit_mode && millis() > lastDebounceTime + idleDelay) {
    edit_mode = false;
    draw();
  }
}

void loop() {                
  debounce();
  read_analog();
  check_idle();
}
