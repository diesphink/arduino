#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <Fonts/Picopixel.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int VRx = A0;
int VRy = A1;
int SW = 2;

struct ITEM {
  String label;
  String extra;
  byte value;
  byte max;
};

ITEM menu[6] = { 
  { "HP", "", 56, 58},
  { "Spells", "Short rest", 1, 2},
  { "Healing", "Long rest", 4, 9 },
  { "Hit Dice", "Long rest (4)", 0, 8 },
  { "Pact Keeper", "Long rest", 0, 1 },
  { "Fire Staff", "1d6 at dawn", 3, 10 }
};

byte MAX_MENU = 5;

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
int debounceDelay = 50;    // the debounce time; increase if the output flickers
int idleDelay = 15000;    // the debounce time; increase if the output flickers

void debounce() {

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
  // initialize with the I2C addr 0x3C
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

//  Serial.begin(9600); 

  // PINs do joystick
  pinMode(VRx, INPUT);
  pinMode(VRy, INPUT);
  pinMode(SW, INPUT_PULLUP);
  
  for (byte i = 0; i < 6; i++)
    menu[i].value = EEPROM.read(i);

}

void draw_menu_item(ITEM item, byte y, bool selected) {
  if (selected)
    display.fillRect(0, y, 128, 8, WHITE);
  display.setTextColor(selected ? BLACK : WHITE, selected ? WHITE : BLACK);
  if (item.label.length() > 8) {
    display.setFont(&Picopixel);
    display.setCursor(2, y + 5);
  } else {
    display.setFont();
    display.setCursor(2, y);
  }
  display.print(item.label);
  for (byte i = 0; i < item.max; i++) {
    if (item.value > i) { 
      display.drawRoundRect(52 + 7 * i,     y + 1,     8, 6, 2, selected ? BLACK : WHITE);
      display.fillRect     (52 + 7 * i + 2, y + 1 + 2, 4, 2,    selected ? BLACK : WHITE);
    } else
      display.drawRoundRect(52 + 7 * i,     y + 1,     8, 6, 2, selected ? BLACK : WHITE);
  }
}

void draw() {

  bool selected = edit_mode && selected_val == 0;

  display.clearDisplay();  
  if (selected)
    display.fillRect(0, 0, 128, 16, WHITE);
  display.setTextSize(1);
  display.setTextColor(selected ? BLACK : WHITE);
  display.setFont();

  
  if (edit_mode && selected_val > 0) {
    display.setCursor(2, 0);
    display.println("Recover");
    display.setCursor(2, 8);
    display.println(menu[selected_val].extra);
  } else {
    // Nome
    display.setCursor(2, 0);
    display.print("Sig Saranova");
  
    // HP Bar
    display.drawRoundRect(0, 9, 128, 6, 2, selected ? BLACK : WHITE);
    display.fillRoundRect(2, 11, (menu[0].value * 124)/ menu[0].max, 2, 0, selected ? BLACK : WHITE);
  }

  // Valores
  byte x = 110;
  if (menu[edit_mode ? selected_val : 0].value > 9)
    x -= 6;
  if (menu[edit_mode ? selected_val : 0].max > 9)
    x -= 6;
  display.setCursor(x, 0);
  
  display.print(menu[edit_mode ? selected_val : 0].value);
  display.print("/");
  display.print(menu[edit_mode ? selected_val : 0].max);

  for (byte i = 1; i < 6; i++)
    draw_menu_item(menu[i], 8 + i * 8, edit_mode && selected_val == i);
  
  display.display();
  display.clearDisplay();  
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
      if (left && menu[selected_val].value > 0) {
        menu[selected_val].value = menu[selected_val].value - 1;
      }
      if (right && menu[selected_val].value < menu[selected_val].max) {
        menu[selected_val].value = menu[selected_val].value + 1;
      }
      EEPROM.write(selected_val, menu[selected_val].value);
      draw();

      // HP tem um delay menor que as outras opções
      delay(selected_val == 0 ? 100: 300);
    }

    if (left or right or up or down)
      lastDebounceTime = millis();
  }
}

void check_idle() {
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
