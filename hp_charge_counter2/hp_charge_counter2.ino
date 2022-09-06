/*
 * HP Charge counter
 *
 * This program controls via push buttons a few variables in a menu,
 * showing in an oled screen
 *
 */

#include <EEPROM.h>
#include <Tiny4kOLED.h>
#include <TinyI2CMaster.h>

// PIN definitions
const byte PIN_INPUT_BTNS = A2;
// on arduino, SCK -> A5, SDA -> A4
// on attiny85, SCK -> pin7, SDA -> pin5

// Menu structure
struct ITEM {
  String label;
  String extra;
  byte max;
};

// Menu variables
const byte MENU_COUNT = 6;
ITEM menu[MENU_COUNT];
byte values[MENU_COUNT] = {};

// Menu flow control
bool screen_state = true;
bool edit_mode = false;
byte selected_val = 0;

// Pressed buttons
const byte BTN_NONE = -1;
const byte BTN_UP = 0;
const byte BTN_RIGHT = 1;
const byte BTN_DOWN = 2;
const byte BTN_LEFT = 3;

// Horizontal position to draw values
const byte VAL_POS = 82;

// Debounce control
byte buttonState;
byte lastButtonState = BTN_NONE;
unsigned long lastDebounceTime = 0;

// ===============
// SETUP
// ===============
void setup() {
  // Prepare pin to read buttons as analog input
  pinMode(PIN_INPUT_BTNS, INPUT);

  // Cria as opções de menu
  menu[0] = {F("HP"), F(""), 30};
  menu[1] = {F("Ki"), F("1 on short, long"), 5};
  menu[2] = {F("Ki/2"), F("Long rest"), 2};
  menu[3] = {F("Curse Mantra"), F("Short rest"), 1};
  menu[4] = {F("Hit dice"), F("Long rest (2)"), 3};
  menu[5] = {F("Inspiration"), F(""), 1};

  // Read saved values from EEPROM
  for (byte i = 0; i < MENU_COUNT; i++) {
    values[i] = EEPROM.read(i);
    if (values[i] > menu[i].max)
      values[i] = menu[i].max;
  }

  // Initialize OLED
  oled.begin(0, 0, 128, 68, sizeof(tiny4koled_init_128x64r), tiny4koled_init_128x64r);
  oled.enableChargePump();
  oled.setPages(8);
  oled.setFont(FONT6X8);
  oled.clear();
  oled.on();

  // Draw screen
  draw();
}

// ===============
// DRAW INFO
// ===============
void draw() {
  oled.invertOutput(false);
  byte chrs_valores = 3;
  if (values[edit_mode ? selected_val : 0] >= 100)
    chrs_valores += 2;
  else if (values[edit_mode ? selected_val : 0] >= 10)
    chrs_valores += 1;
  if (menu[edit_mode ? selected_val : 0].max >= 100)
    chrs_valores += 2;
  else if (menu[edit_mode ? selected_val : 0].max >= 10)
    chrs_valores += 1;

  // First two lines
  // When not editing, or is editing the first line, shows name and current/max HP
  // When is editing any other line, shows recover line and current/max value
  if (!edit_mode || selected_val == 0) {
    // Name
    oled.invertOutput(edit_mode && selected_val == 0);
    oled.setCursor(0, 0);
    oled.startData();
    oled.repeatData(0b00000000, 2);
    oled.endData();
    oled.setCursor(2, 0);
    oled.print(F("Quimera Cansada"));

    // Current/max HP
    oled.startData();
    oled.repeatData(0b00000000, 54 - chrs_valores * 6);
    oled.endData();
    oled.setCursor(128 - chrs_valores * 6, 0);
    oled.print(values[edit_mode ? selected_val : 0]);
    oled.print(F("/"));
    oled.print(menu[edit_mode ? selected_val : 0].max);

    // HP Bar
    oled.setCursor(0, 1);
    oled.startData();
    oled.sendData(0b00111100);
    oled.sendData(0b01000010);
    oled.repeatData(0b01011010, (values[0] * 124) / menu[0].max);
    oled.repeatData(0b01000010, 124 - (values[0] * 124) / menu[0].max);
    oled.sendData(0b01000010);
    oled.sendData(0b00111100);
    oled.endData();

  } else {
    // Recover
    oled.invertOutput(false);
    oled.setCursor(0, 0);
    oled.startData();
    oled.repeatData(0b00000000, 2);
    oled.endData();
    oled.setCursor(2, 0);
    oled.print(F("Recover"));

    // Current/max value
    oled.startData();
    oled.repeatData(0b00000000, 84 - chrs_valores * 6);
    oled.endData();
    oled.setCursor(128 - chrs_valores * 6, 0);
    oled.print(values[edit_mode ? selected_val : 0]);
    oled.print(F("/"));
    oled.print(menu[edit_mode ? selected_val : 0].max);

    // When recover
    oled.setCursor(0, 1);
    oled.startData();
    oled.repeatData(0b00000000, 128);
    oled.endData();
    oled.setCursor(0, 1);
    oled.print(menu[selected_val].extra);
  }

  // Shows every line on menu from the second one
  for (byte im = 1; im < MENU_COUNT; im++) {
    oled.invertOutput(edit_mode && selected_val == im);
    oled.setCursor(0, im + 1);
    oled.startData();
    oled.repeatData(0b00000000, 2);
    oled.endData();

    oled.setCursor(2, im + 1);
    oled.print(menu[im].label);

    oled.startData();
    oled.repeatData(0b00000000, VAL_POS - menu[im].label.length() * 6);
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
    oled.repeatData(0b00000000, 128 - VAL_POS - 2 - menu[im].max * 7 - 1);
    oled.endData();
  }
}

// ===============
// LOOP - detect button press and idle
// ===============
void loop() {
  // how much (ms) to debounce
  int debounceDelay = 40;

  // how much (ms) to repeat keypress
  int repeatDelay = 120;

  // Time for idle (stops editing) and sleep (screen off)
  int idleDelay = 10 * 1000;
  int offDelay = 15 * 1000;

  // Read the value from analog to determine which pin is pressed
  int analogValue = analogRead(PIN_INPUT_BTNS);

  // Max is 1023, each button in order has a combined 10K ohm resistor
  // So UP will read ~1023,
  // RIGHT will read ~950,
  // DOWN will read ~870,
  // and LEFT will read ~800
  byte btnRead;
  if (analogValue < 700)
    btnRead = BTN_NONE;
  else if (analogValue < 830)
    btnRead = BTN_LEFT;
  else if (analogValue < 900)
    btnRead = BTN_DOWN;
  else if (analogValue < 970)
    btnRead = BTN_RIGHT;
  else
    btnRead = BTN_UP;

  // if button read is different than last read, update last debounce time to millis
  // This way, we can wait for the button to stabilize for at least debounceDelay
  if (btnRead != lastButtonState)
    lastDebounceTime = millis();
  int fromLastDebounce = (millis() - lastDebounceTime);

  // if button is stable for at least debounceDelay, update buttonStante
  // if there's a button, will fire button_pressed, BUT:
  // if it's a button already pressed, will wait at least repeatDelay
  // on fire, will update lastDebounceTime to hold new buttons, repeat button, and idle check
  if (fromLastDebounce > debounceDelay) {
    buttonState = btnRead;
    if (buttonState != BTN_NONE)
      if (buttonState != lastButtonState || fromLastDebounce > repeatDelay) {
        button_pressed();
        lastDebounceTime = millis();
      }
  }

  // and then, last button state will be what we have read
  lastButtonState = btnRead;

  // Check delay for idle
  if (fromLastDebounce > idleDelay) {
    edit_mode = false;
    draw();
  }

  // Check delay for off
  if (fromLastDebounce > offDelay) {
    oled.off();
    screen_state = false;
  }
}

// When a button is pressed
void button_pressed() {
  // If the screen is off, just turn it on
  if (screen_state == false) {
    screen_state = true;
    oled.on();
    draw();

    // if it's not editing, switch to edit mode
  } else if (edit_mode == false) {
    edit_mode = true;
    draw();

    // if it's already editing
  } else {
    // Create a few variables with directions
    bool left = buttonState == BTN_LEFT;
    bool right = buttonState == BTN_RIGHT;
    bool up = buttonState == BTN_UP;
    bool down = buttonState == BTN_DOWN;

    // Either up or down will scroll on the menu
    if (up || down) {
      if (up && selected_val == 0)
        selected_val = MENU_COUNT - 1;
      else if (down && selected_val == MENU_COUNT - 1)
        selected_val = 0;
      else
        selected_val += up ? -1 : +1;

      draw();
    }

    // Either left or right will update values
    if (left or right) {
      if (left && values[selected_val] > 0)
        values[selected_val] = values[selected_val] - 1;
      if (right && values[selected_val] < menu[selected_val].max)
        values[selected_val] = values[selected_val] + 1;

      EEPROM.write(selected_val, values[selected_val]);
      draw();
    }
  }
}
