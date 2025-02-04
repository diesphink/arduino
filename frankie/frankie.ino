//Programa: Frankie
//Autor: Diego Pereyra

// Inspirado em: 
//  - https://raw.githubusercontent.com/RuiSantosdotme/Random-Nerd-Tutorials/master/Projects/ESP/ESP_Telegram/ESP_Telegram_Control_Outputs.ino
// NTP Sync daqui:
//  - https://raw.githubusercontent.com/PaulStoffregen/Time/refs/heads/master/examples/TimeNTP_ESP8266WiFi/TimeNTP_ESP8266WiFi.ino

// TODO
//  - [ ] NTP num arquivo próprio
//  - [ ] Usar EEPROM para salvar os dados de cfg do horário, usar valores em minutos ao invés do atual
//  - [ ] Usar o telegram para definir os dados de cfg de horário
//  - [ ] Aumentar o intervalo de comm com telegram, mas quando tiver interação com telegram, diminuir o intervalo para 5s por ~ 1m
//  - [ ] Ajustar outras cfgs para serem também parametrizadas/salvas na eeprom (e.g. snooze dos avisos, tempo de checagem do telegram, etc)
//  - [ ] Ajustar para poder definir e.g. +30 ao invés de um horário
//  - [ ] Ajustar para a quantidade de remédios ser configurável
//  - [ ] Se o horário é menor que o alarme, mas já confirmou algum alarme no futuro, então tá atrasado (controle de pós meia noite)

#include <Wire.h>
#include "SSD1306Wire.h" // and ESP32 OLED Driver for SSD1306 displays by ThingPulse: https://github.com/ThingPulse/esp8266-oled-ssd1306
 
#include <WiFiUdp.h>
#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>   // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <TimeLib.h> // Time by miguel Morgolis

#include "images.h"

// =========================
// SECRETS
// create secrets.h with the content below
// =========================

#include "secrets.h"

// const char* ssid = "*****";
// const char* pass = "*****";
// #define BOTtoken "***:***-***"  // your Bot Token (Get from Botfather)

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
// #define CHAT_ID "***"

// =========================
// TELEGRAM
// =========================

WiFiClientSecure client;

#ifdef ESP8266
  X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

UniversalTelegramBot bot(BOTtoken, client);

// Checks for new messages every 1 second.
int botRequestDelay = 5 * 1000;
unsigned long lastTimeBotRan;

// =========================
// DISPLAY
// =========================

//Pinos do NodeMCU
// SDA => D5
// SCL => D6
// Inicializa o display Oled
SSD1306Wire  display(0x3c, D5, D6);
bool dirty;

// =========================
// BOTÃO
// =========================

// pino do botão
const int buttonPin = D1;

int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin

// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 10;    // the debounce time; increase if the output flickers

// =========================
// LÓGICA DO FRANKIE
// =========================

// consts
const int STATUS_INIT = -1;
const int STATUS_OK = 0;
const int STATUS_DONE = 1;
const int STATUS_LATE = 2;

// CFGs
int alarmHour[] = {2, 3, 5};
int alarmMinute[] = {0, 30, 47};

// state control
int currentStatus = STATUS_INIT;  // Status atual
int currentAlarm = 0;             // Qual o próximo alarme
time_t currentTime = 0;           // Hora atual
long lastAlert = 0;               // Momento do último alerta

void show_display(String text1, bool filled);
void show_display(String text1, String text2, bool filled);
void show_display(int lines, String text1, String text2, bool filled);

void sendMsg(String text) {
  show_display("-- rede --", "Mensagem", false);

  Serial.println("Sending message: " + text);
  bot.sendMessage(CHAT_ID, text);
  refreshDisplay();
}

void getMsg() {
  show_display("-- rede --", "Checando", false);

  Serial.println("Checking telegram for messages");
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while(numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
  lastTimeBotRan = millis();

  refreshDisplay();
}

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("Mensagens recebidas");
  Serial.println(" - Qtd: " + String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      sendMsg("🚫 Unauthorized user");
      continue;
    }
    
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    Serial.println(" - Txt: " + text);

    if (text == "/start") {
      String welcome = "👋 Olá, **" + from_name + "** eu sou o **Frankie!**\n";
      welcome += "Estou aqui para te ajudar a lembrar de tomar os seus remédios, para isso você pode usar os comandos abaixo:\n\n";
      welcome += "/1  \n";
      welcome += "/led_off to turn GPIO OFF \n";
      welcome += "/state to request current GPIO state \n";
      sendMsg(welcome);
    }

    if (text == "/state") {
      sendMsg("LED is ON");
    }
  }
  Serial.println("");
}

void refreshDisplay() {
  if (currentStatus == STATUS_INIT) {
    show_display("Inicializando...", true);
  } else if (currentStatus == STATUS_OK) {
    show_display(
      String(currentAlarm + 1) + "º remédio", 
      "até " + String(alarmHour[currentAlarm]) + "h" + (alarmMinute[currentAlarm] > 0 ? String(alarmMinute[currentAlarm]): "") +"!", 
      false);
  } else if (currentStatus == STATUS_LATE) {
    show_display(
        "ATRASADA!",
        "Era até " + String(alarmHour[currentAlarm]) + "h" + (alarmMinute[currentAlarm] > 0 ? String(alarmMinute[currentAlarm]): "") +"!",
        true);
    display.setColor(WHITE);
  
  } else if (currentStatus == STATUS_DONE) {
    show_display("OK!", false);
  }
}

void updateStatus(int status) {
  if (currentStatus != status) {
    currentStatus = status;
    dirty = true;
  }
}

void handleButtonPress() {

  if (currentStatus == STATUS_DONE) {
    currentAlarm = 0;
    updateStatus(STATUS_OK);
  } else if (currentAlarm == 2) {
    updateStatus(STATUS_DONE);
  } else {
    currentAlarm++;
  }

  if (currentAlarm == 1) {
    alarmHour[1] = hour(currentTime);
    alarmMinute[1] = minute(currentTime) + 30;
    if (alarmMinute[1] > 60) {
      alarmHour[1] = alarmHour[1] + 1;
      alarmMinute[1] = alarmMinute[1] - 60;
    }
    
  }

  dirty = true;
}

void checkLate() {
  if (currentStatus == STATUS_DONE)
    return;

  currentTime = now();
  long currentMins = hour(currentTime) * 60 + minute(currentTime);
  long target = alarmHour[currentAlarm] * 60 + alarmMinute[currentAlarm];

  if (currentMins > target) {
    updateStatus(STATUS_LATE);
  } else {
    updateStatus(STATUS_OK);
    
  }
}

void checkAlert() {
  if (currentStatus == STATUS_LATE) {
    if (lastAlert == 0 || (millis() - lastAlert) > 30 * 60 * 1000) {
      lastAlert = millis();
      Serial.println("Sending new alert");
      sendMsg("⚠️ Você deveria ter tomado remédio!");
    }
  }
}


void checkButtonPress() {
  // read the state of the switch into a local variable
  int reading = digitalRead(buttonPin);

  if(reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:

    // if the button state has changed:
    if(reading != buttonState) {
      buttonState = reading;

      // only run on high
      if(buttonState == HIGH) {
        handleButtonPress();
      }
    }
  }

  lastButtonState = reading;
}

void checkTelegram() {
  if (millis() > lastTimeBotRan + botRequestDelay)
    getMsg();
}

void setup()
{
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // button pin
  pinMode(buttonPin, INPUT);
  
  // Init display
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  refreshDisplay();

  #ifdef ESP8266
    configTime(0, 0, "pool.ntp.org");      // get UTC time via NTP
    client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
  #endif

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  
  #ifdef ESP32
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  #endif

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());

  setupNTP();

  Serial.println("Sending test");
  sendMsg("🕓 Oi, acabei de ligar.\n\nMeu horário atual é: " + String(hour(now())) + ":" + String(minute(now())));

  refreshDisplay();
}

void loop()
{
  dirty = false;

  checkLate();
  checkAlert();
  checkButtonPress();
  checkTelegram();

  if (dirty)
    refreshDisplay();
}
