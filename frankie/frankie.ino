//Program: Frankie
//Author: Diego Pereyra

// Inspired by: 
//  - https://raw.githubusercontent.com/RuiSantosdotme/Random-Nerd-Tutorials/master/Projects/ESP/ESP_Telegram/ESP_Telegram_Control_Outputs.ino
// NTP Sync from here:
//  - https://raw.githubusercontent.com/PaulStoffregen/Time/refs/heads/master/examples/TimeNTP_ESP8266WiFi/TimeNTP_ESP8266WiFi.ino

// TODO
//  - [X] NTP num arquivo próprio
//  - [X] Usar EEPROM para salvar os dados de cfg do horário, usar valores em minutos ao invés do atual
//  - [X] Usar o telegram para definir os dados de cfg de horário
//  - [X] Aumentar o intervalo de comm com telegram, mas quando tiver interação com telegram, diminuir o intervalo para 5s por ~ 1m
//  - [ ] Ajustar outras cfgs para serem também parametrizadas/salvas na eeprom (e.g. snooze dos avisos, tempo de checagem do telegram, etc)
//  - [X] Ajustar para poder definir e.g. +30 ao invés de um horário
//  - [ ] Ajustar para a quantidade de remédios ser configurável
//  - [X] Se o horário é menor que o alarme, mas já confirmou algum alarme no futuro, então tá atrasado (controle de pós meia noite)

#include <Wire.h>
#include "SSD1306Wire.h" // and ESP32 OLED Driver for SSD1306 displays by ThingPulse: https://github.com/ThingPulse/esp8266-oled-ssd1306
 
/* 
  Set true if you want use external library for SSL connection instead ESP32@WiFiClientSecure 
  For example https://github.com/OPEnSLab-OSU/SSLClient/ is very efficient BearSSL library.
  You can use AsyncTelegram2 even with other MCUs or transport layer (ex. Ethernet)
  With SSLClient, be sure "certificates.h" file is present in sketch folder
*/ 
#define USE_CLIENTSSL true  

#include <AsyncTelegram2.h>

// Timezone definition
#include <time.h>
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"

#ifdef ESP8266
  #include <ESP8266WiFi.h>
  BearSSL::WiFiClientSecure client;
  BearSSL::Session   session;
  BearSSL::X509List  certificate(telegram_cert);
  
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClient.h>
  #if USE_CLIENTSSL
    #include <SSLClient.h>  
    #include "tg_certificate.h"
    WiFiClient base_client;
    SSLClient client(base_client, TAs, (size_t)TAs_NUM, A0, 1, SSLClient::SSL_ERROR);
  #else
    #include <WiFiClientSecure.h>
    WiFiClientSecure client;  
  #endif
#endif

// NTP
#include <WiFiUdp.h>

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

AsyncTelegram2 bot(client);
int64_t defaultChatId = CHAT_ID_MA;

// =========================
// DISPLAY
// =========================

//NodeMCU display pins
// SDA => D5
// SCL => D6
SSD1306Wire  display(0x3c, D5, D6);
bool display_dirty = true;
bool cfg_dirty = false;

// =========================
// BUTTON
// =========================

// button PIN
const int buttonPin = D0; // S3

int buttonState;                  // the current reading from the input pin
int lastButtonState = LOW;        // the previous reading from the input pin

// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long lastDebounceTime = 0;        // the last time the output pin was toggled
long debounceDelay = 10;          // the debounce time; increase if the output flickers

// =========================
// EEPROM
// =========================

const int ADDR_CURRENT = 8;           // 4 bytes pro currentAlarm
const int ADDR_LASTHECK = 12;         // 4 bytes pro lastCheck
const int ADDR_ALARMS = 32;           // 8 bytes pra cada alarme

const int SIZE_OF_ALARM = 8;

// =========================
// FRANKIE'S LOGIC
// =========================

bool debug = false;

// consts
const unsigned int STATUS_OK = 0;
const unsigned int STATUS_DONE = 1;
const unsigned int STATUS_LATE = 2;

const unsigned int TYPE_ABSOLUTE = 0; // Alarm holds time to alert (in minutes from 0h)
const unsigned int TYPE_RELATIVE = 1; // Alarm holds minutes from last check

// structs
struct alarmData                      // Struct to define the alarm layout
{
  unsigned int type;                  // Type o alarm (ABSOLUTE/RELATIVE)
  unsigned int minutes;               // Quantity of minutes

};

// CFGs
alarmData alarms[3]{
  {TYPE_ABSOLUTE, 71},
  {TYPE_RELATIVE, 30},
  {TYPE_ABSOLUTE, 243}
};

// state control
int currentStatus = STATUS_OK;        
int currentAlarm = 0;                 // Which is the current alarm index (0-2)
long lastAlert = 0;                   // When was the last alert sent in seconds (now())
                                      // Used to calculate if we can send new messages while status is LATE

int lastCheck = 0;                    // When was the last check made, in minutes Has two uses: 
                                      // 1. Determine next alarm for relative alarms
                                      // 2. Determine wether we are still on the "same day", when
                                      // lastCheck > currentTime. Last Check is resetted every time we
                                      // cycle from DONE to OK

time_t currentTime = 0;               // Holds the current time of execution, updated at each loop

// =========================
// TELEGRAM FUNCTIONS
// =========================
void sendMsg(String text) {
  sendMsg(defaultChatId, text);
}

void sendMsg(int64_t chatid, String text) {
  Serial.println("Sending message: " + text);
  bot.sendTo(chatid, text);
  display_dirty = true;
}

// Handle what happens when you receive new messages
void handleNewMessage(TBMessage msg) {
  Serial.println("Messages received");

  // Allow only mine and ma's
  if (msg.chatId != CHAT_ID_MA && msg.chatId != CHAT_ID_DIEGO){
    sendMsg(msg.chatId, "🚫 Usuário não autorizado");
    return;
  }

  String text = msg.text;
  String from_name = msg.sender.firstName;

  Serial.println(" - Txt: " + text);

  if (text == "/start" || text == "/help") {
    String welcome = "👋 Olá, " + from_name + ", eu sou o Frankie!\n";
    welcome += "Estou aqui para te ajudar a lembrar de tomar os seus remédios, para isso você pode usar os comandos abaixo:\n\n";
    welcome += "/status para ver o status atual  \n";
    welcome += "/btn para agir como se tivesse apertado o botão\n";
    welcome += "/set {alarme} {valor} para definir um alarme, o valor pode ser relativo em minutos (e.g. +30) ou um horário (e.g. 10:30, 8h, 22h15)\n";
    welcome += "\n";
    welcome += "Qualquer outro texto recebido indicará pressionamento do botão";
    sendMsg(msg.chatId, welcome);

  } else if (text == "/debug") {
    debug = !debug;

  } else if (text.startsWith("/add")) {
    long delta = text.substring(4).toInt() * 60;

    setTime(now() + delta);
    currentTime = now();
    Serial.println("Delta: " + String(delta));
    Serial.println("New date: " + String(year(now())) + "-" + String(month(now())) + "-" + String(day(now())) + " " + currentTimeFormatted());
    checkStatus();
    display_dirty = true;

  } else if (text == "/status") {
    String status = genStatusText() + "\n\n";
    status += genAlarmTable() + "\n";
    status += genTimeText();
    sendMsg(msg.chatId, status);

  } else if (text.startsWith("/set")) {
    int index;
    alarmData alarm = {0, 0};
    text = text.substring(4);
    text.trim();
    
    index = text[0] - '0' - 1;

    if (index < 0 || index > 2) {
      sendMsg(msg.chatId, "🚫 Índice inválido para os alarmes (" + String(text[0])+ ")");
      return;
    }

    text = text.substring(1);
    text.trim();

    if (text.length() == 0) {
      sendMsg(msg.chatId, "🚫 Valor inválido para o alarme (" + text + ")");
      return;
    }

    if (text[0] == '+') {
      alarm.type = TYPE_RELATIVE;
      alarm.minutes = text.substring(1).toInt();
      alarms[index] = alarm;
      saveAlarms(index);
      sendMsg(msg.chatId, "✅ Alarme " + String(index + 1) + " definido para +" + alarm.minutes);
    } else {
      alarm.type = TYPE_ABSOLUTE;

      int hour, minute;
      int pos = text.indexOf(":");
      if (pos == -1)
        pos = text.indexOf("h");
      
      if (pos == -1) {
        hour = text.toInt();
        minute = 0;
      } else {
        hour = text.substring(0, pos).toInt();
        minute = text.substring(pos+1, text.length()).toInt();
      }
      alarm.minutes = hour * 60 + minute;
      alarms[index] = alarm;
      saveAlarms(index);
      sendMsg(msg.chatId, "✅ Alarme " + String(index + 1) + " definido para " + formatMinutes(alarm.minutes));
    }

  } else {
    //  also same for if (text == "/btn") {
    handleButtonPress();
    checkStatus();
    sendMsg(msg.chatId, genStatusText());

  }
  Serial.println("");
}

String genAlarmTable() {
  String status = "";
  for (int i = 0; i <= 2; i++) {
    alarmData alarm = alarms[i];
    if (currentAlarm > i || currentStatus == STATUS_DONE)
      status += "🔳 ";
    else
      status += "⬜️ ";
    if (alarm.type == TYPE_RELATIVE) {
      status += "+" + String(alarm.minutes);
      if (currentAlarm == i)
        status += " (" + currentAlarmFormatted() + ")";
    } else
      status += formatMinutes(alarm.minutes);
    status += "\n";
  }
  return status;
}

String genStatusText() {
    if (currentStatus == STATUS_DONE)
      return "✅ Tudo certo!";
    if (currentStatus == STATUS_LATE)
        return "⚠️ ATRASADA! Era para ter tomado o " + String(currentAlarm + 1) + "º remédio até " + currentAlarmFormatted() + "!";
    if (currentStatus == STATUS_OK)
        return "⏰ Tomar o " + String(currentAlarm + 1) + "º remédio até " + currentAlarmFormatted();
    return "Status desconhecido!";
}

String genTimeText() {
  return "Horário atual: " + currentTimeFormatted();
}

String genBootupText() {
  return "ℹ️ Oi, acabei de ligar\n\n" + genAlarmTable() + "\n" + genTimeText();
}

// =========================
// TIME RELATED FUNCTIONS
// =========================

int currentTimeInMinutes() {
  int minutes = hour(currentTime) * 60 + minute(currentTime);
  // Se os minutes for menor que a última checagem, soma 24h
  // Indica que ainda estamos resolvendo pendências ae do dia anterior
  // Mas quando terminar (DONE), pode voltar a contar normal
  if (minutes < lastCheck && currentStatus != STATUS_DONE)
    minutes += 24 * 60;
  return minutes;
  
}

int currentAlarmInMinutes() {
  return alarmInMinutes(alarms[currentAlarm]);
}

int alarmInMinutes(alarmData alarm) {
  if (alarm.type == TYPE_ABSOLUTE)
    return alarm.minutes;
  else {
    return lastCheck + alarm.minutes;
  }
}

String currentAlarmFormatted() {
  return formatMinutes(currentAlarmInMinutes());
}

String leftPad(int number) {
  if (number < 10)
    return "0" + String(number);
  else
    return String(number);
}

String formatMinutes(int minutes) {
  int h = minutes/60;
  int m = minutes - h * 60;
  if (!debug)
    h = h % 24;

  return leftPad(h) + "h" + leftPad(m);
}

String currentTimeFormatted() {
  return timeFormatted(currentTime);
}

String timeFormatted(long time) {
  return leftPad(hour(time)) + "h" + leftPad(minute(time));
}

// =========================
// DISPLAY FUNCTIONS
// =========================

void refreshDisplay() {
  if (currentStatus == STATUS_OK) {
    show_display(
      String(currentAlarm + 1) + "º remédio", 
      "até " + currentAlarmFormatted() +"", 
      false);

  } else if (currentStatus == STATUS_LATE) {
    show_display(
        "ATRASADA!!",
        "Era até " + currentAlarmFormatted() +"!",
        true);

  } else if (currentStatus == STATUS_DONE) {
    show_display("OK!", false);
  }
}

// =========================
// FLOW CONTROL
// =========================

void updateStatus(int status) {
  if (currentStatus != status) {
    if (currentStatus == STATUS_DONE && status == STATUS_OK) {
      lastCheck = 0;
      currentAlarm = 0;
    }
    if (status == STATUS_DONE) {
      currentAlarm = 0;
    }
    currentStatus = status;
    cfg_dirty = true;
    display_dirty = true;
  }
}

void handleButtonPress() {
  if (currentStatus == STATUS_DONE) {
    updateStatus(STATUS_OK);
  } else if (currentAlarm == 2) {
    updateStatus(STATUS_DONE);
    lastCheck = currentTimeInMinutes();
  } else {
    currentAlarm++;
    lastCheck = currentTimeInMinutes();
  }
  cfg_dirty = true;
  display_dirty = true;
}

void checkStatus() {
  if (currentStatus == STATUS_DONE) {
    // Check wether we are before the first alarm to zero (goes back to OK)
    // Give a little time (5m) since check, as not to zero out of nowhere and looks like it's an
    // error, so it leaves the "OK!" for a little while
    if (currentTimeInMinutes() < currentAlarmInMinutes() && currentTimeInMinutes() >= lastCheck + 5) {
      updateStatus(STATUS_OK);
    }
  } else {
    if (currentTimeInMinutes() >= currentAlarmInMinutes()) {
      updateStatus(STATUS_LATE);
    } else {
      updateStatus(STATUS_OK);   
    }
  }
}

// =========================
// DEBOUNCES (button, telegram, alert)
// =========================

void checkAlert() {
  if (currentStatus == STATUS_LATE) {
    if (lastAlert == 0 || (now() - lastAlert) > 30 * 60) {
      lastAlert = now();
      Serial.println("Sending new alert");
      sendMsg(genStatusText());
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
  TBMessage msg;
  while (bot.getNewMessage(msg))
    handleNewMessage(msg);
}

// =========================
// EEPROM FUNCTIONS
// =========================

void saveCfg() {
  bool changed = false;
  if (EEPROM.read(ADDR_CURRENT) != currentAlarm) {
    EEPROM.put(ADDR_CURRENT, currentAlarm);
    changed = true;
  }
  if (EEPROM.read(ADDR_LASTHECK) != lastCheck) {
    EEPROM.put(ADDR_LASTHECK, lastCheck);
    changed = true;
  }
  if (changed)
    EEPROM.commit();
}

void saveAlarms(int index) {
  EEPROM.put(ADDR_ALARMS + index * SIZE_OF_ALARM, alarms[index]);
  EEPROM.commit();
}

void load() {
  EEPROM.get(ADDR_CURRENT, currentAlarm);
  EEPROM.get(ADDR_LASTHECK, lastCheck);
  for (int i = 0; i < 3; i++) {
    EEPROM.get(ADDR_ALARMS + i * SIZE_OF_ALARM, alarms[i]);
  }

}

// =========================
// SETUP
// =========================

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
  show_display("Inicializando...", true);

  EEPROM.begin(512);
  delay(5000);
  load();

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  delay(500);

  #ifdef ESP8266
    // Sync time with NTP, to check properly Telegram certificate
    configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
    //Set certficate, session and some other base client properies
    client.setSession(&session);
    client.setTrustAnchors(&certificate);
    client.setBufferSizes(1024, 1024);
  #elif defined(ESP32)
    // Sync time with NTP
    configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
    #if USE_CLIENTSSL == false
      client.setCACert(telegram_cert);
    #endif
  #endif
    
  setupNTP();
  // Set the Telegram bot properies
  bot.setUpdateTime(2000);
  bot.setTelegramToken(BOTtoken);

  // Check if all things are ok
  Serial.print("\nTest Telegram connection... ");
  bot.begin() ? Serial.println("OK") : Serial.println("NOK");

  currentTime = now();

  sendMsg(genBootupText());

  sendMsg(String(millis()));

  
}

// =========================
// LOOP
// =========================

void loop()
{
  currentTime = now();

  checkStatus();
  checkAlert();
  checkButtonPress();
  checkTelegram();

  if (display_dirty)
    refreshDisplay();
  display_dirty = false;

  if (cfg_dirty)
    saveCfg();
}
