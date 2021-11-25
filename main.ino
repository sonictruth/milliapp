#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_INA219.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <AceButton.h>
#include "secrets.h"

using namespace ace_button;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

const uint8_t BUTTON_PIN = 15;
const String logFileName =  "/log.txt";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_INA219 ina219;
AceButton button(BUTTON_PIN);
WebServer server(80);
void handleEvent(AceButton*, uint8_t, uint8_t);
File file;
float lastCurrent_mA = 0;

bool isLowPower = false;
uint32_t lasttime =  millis();
uint8_t lowPowerDelaySeconds = 5;

void setup() {
  Serial.begin(115200);
  initCpu();
  initDisplay();
  initNetwork();
  initOTA();
  initFS();
  initButtons();
  initINA219();
}

void initOTA() {
  ArduinoOTA
  .onStart([]() {
    display.clearDisplay();
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    writeToDisplay("Update " + type);
  })
  .onEnd([]() {
    writeToDisplay("End");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    writeToDisplay("Progress: " + String((progress / (total / 100))) + "%...");
  })
  .onError([](ota_error_t error) {
    writeToDisplay("Error: " + String( error));
    if (error == OTA_AUTH_ERROR) writeToDisplay("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) writeToDisplay("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) writeToDisplay("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) writeToDisplay("Receive Failed");
    else if (error == OTA_END_ERROR) writeToDisplay("End Failed");
  });

  ArduinoOTA.begin();
}

void initButtons() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  button.setEventHandler(handleEvent);
}

void initINA219() {
  if (! ina219.begin()) {
    writeToDisplay("Failed to find INA219", 0, 0);
    while (1) {
      delay(10);
    }
  }
  // ina219.setCalibration_32V_1A();
}

void initCpu() {
  if (isLowPower) {
    setCpuFrequencyMhz(40);
  } else {
    setCpuFrequencyMhz(240);
  }
}

void initNetwork() {
  if (isLowPower) {
    server.stop();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    writeToDisplay("Low power mode!", 0, 1);
  } else {
    writeToDisplay("conect", 0, 1);
    WiFi.mode(WIFI_STA);
    writeToDisplay("Connecting...", 0, 1);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      writeToDisplay("Connection Failed!", 0, 0);
      return;
    }
    delay(5000);
    writeToDisplay("IP: " + ipToString(WiFi.localIP()), 0, 1);
    server.on("/", handle_OnConnect);
    server.begin();
  }
}
String ipToString(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  return s;
}
void initFS() {
  if (!SPIFFS.begin(true)) {
    writeToDisplay("FS Error", 0 , 0);
    return;
  }
  file = SPIFFS.open(logFileName, "a+");
  if (!file) {
    writeToDisplay("Unable to open log file", 0, 0);
    return;
  }
  if (!file.print("|")) {
    writeToDisplay("File error");
  }
}

void initDisplay() {
  Wire.begin(5, 4);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.clearDisplay();
  writeToDisplay("MilliApp", 0, 0);
}

void writeToDisplay(String message) {
  writeToDisplay(message, 0, 0);
}

void writeToDisplay(String message, int col, int row) {
  display.setCursor(10 * col, 10 * row);
  display.print(message);
  display.display();
}

void handle_Clearlog() {
  server.send(200, "text/html", "OK" );
}

void handle_OnConnect() {
  server.send(200, "text/html", "OK" );
}

void handleEvent(AceButton* , uint8_t eventType, uint8_t ) {
  switch (eventType) {
    case AceButton::kEventPressed:
      display.clearDisplay();
      isLowPower = !isLowPower;
      initCpu();
      initNetwork();
      break;
  }
}


void handleINA219(uint32_t time) {
  float current_mA = ina219.getCurrent_mA();
  writeToDisplay("Current: " + String(current_mA) + "mA", 0, 3);
  if (current_mA > 0 && current_mA != lastCurrent_mA) {
    if (!file.print( String(time) + ':' + String(current_mA) + ',')) {
      writeToDisplay("File error");
    }
  }
  lastCurrent_mA = current_mA;
}


void loop() {
  uint32_t now = millis() ;
  if (now - lasttime >= lowPowerDelaySeconds * 1000 || !isLowPower) {
    lasttime = now;
    writeToDisplay("Uptime: " + String(now / 1000) , 0, 0);
    handleINA219(now);
  }

  button.check();
  if (!isLowPower) {
    server.handleClient();
    ArduinoOTA.handle();
  }
}
