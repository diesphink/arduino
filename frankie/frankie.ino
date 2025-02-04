//Programa: Frankie
//Autor: Diego Pereyra

// Inspirado em: 
//  - https://raw.githubusercontent.com/RuiSantosdotme/Random-Nerd-Tutorials/master/Projects/ESP/ESP_Telegram/ESP_Telegram_Control_Outputs.ino
// NTP Sync daqui:
//  - https://raw.githubusercontent.com/PaulStoffregen/Time/refs/heads/master/examples/TimeNTP_ESP8266WiFi/TimeNTP_ESP8266WiFi.ino

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
#include "secrets.h"

// const char* ssid = "*****";
// const char* pass = "*****";
// #define BOTtoken "***:***-***"  // your Bot Token (Get from Botfather)

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
// #define CHAT_ID "***"

// =========================
// NTP
// =========================

WiFiUDP ntpUDP;
unsigned int localPort = 8888;  // local port to listen for UDP packets
const int timeZone = -3;
static const char ntpServerName[] = "us.pool.ntp.org";

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
// NTPClient timeClient(ntpUDP, "time.google.com");

#ifdef ESP8266
  X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

WiFiClientSecure client;
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

int alarmHour[] = {2, 3, 5};
int alarmMinute[] = {0, 30, 47};

long lastAlert = 0;
long lastCheck = 0;
int currentAlarm = 0;

time_t currentTime = 0;

const int STATUS_INIT = -1;
const int STATUS_OK = 0;
const int STATUS_DONE = 1;
const int STATUS_LATE = 2;

int currentStatus = STATUS_INIT;
bool network = false;

// Handle what happens when you receive new messages

void sendMsg(String text) {
  network = true;
  refreshDisplay();
  Serial.println("Sending message: " + text);
  bot.sendMessage(CHAT_ID, text);
  network = false;
  refreshDisplay();
}

void getMsg() {
  network = true;
  refreshDisplay();

  Serial.println("Checking telegram for messages");
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while(numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
  lastTimeBotRan = millis();

  network = false;
  refreshDisplay();
}

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
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);

  if (network) {
    display.drawXbm(0, 0, 128, 64, epd_bitmap_mouse);
    display.drawStringMaxWidth(90, 29, 72, "-- rede --");
  } else if (currentStatus == STATUS_INIT) {
    display.drawXbm(0, 0, 128, 64, epd_bitmap_mouse);
    display.drawStringMaxWidth(90, 29, 72, "...");
  } else if (currentStatus == STATUS_OK) {
    display.drawXbm(0, 0, 128, 64, epd_bitmap_mouse);
    display.drawStringMaxWidth(90, 22, 72, String(currentAlarm + 1) + "º remédio");
    display.drawStringMaxWidth(90, 36, 72, "até " + String(alarmHour[currentAlarm]) + "h" + (alarmMinute[currentAlarm] > 0 ? String(alarmMinute[currentAlarm]): "") +"!");
  } else if (currentStatus == STATUS_LATE) {
    display.drawXbm(0, 0, 128, 64, epd_bitmap_mouse_filled);
    display.setColor(BLACK);
    display.drawStringMaxWidth(90, 22, 72, "ATRASADA!");
    display.drawStringMaxWidth(90, 36, 72, "Era até " + String(alarmHour[currentAlarm]) + "h" + (alarmMinute[currentAlarm] > 0 ? String(alarmMinute[currentAlarm]): "") +"!");
    display.setColor(WHITE);
  
  } else if (currentStatus == STATUS_DONE) {
    display.drawXbm(0, 0, 128, 64, epd_bitmap_mouse);
    display.drawStringMaxWidth(90, 29, 72, "OK!");
  }
  for (int i = 0; i < currentAlarm + (currentStatus == STATUS_DONE? 1 : 0); i++) {
    display.fillRect(62 + i * 23, 5, 6, 6);
  }
  display.display();
}

void updateStatus(int status) {
  if (currentStatus != status) {
    currentStatus = status;
    dirty = true;
  }
}

void handleButtonPress() {
  if (currentStatus != STATUS_DONE)
    lastCheck = millis();

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

  Serial.println("Starting UDP");
  ntpUDP.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(ntpUDP.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

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

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (ntpUDP.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = ntpUDP.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  ntpUDP.beginPacket(address, 123); //NTP requests are to port 123
  ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
  ntpUDP.endPacket();
}