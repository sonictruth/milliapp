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
#include <math.h>
#include "secrets.h"
#include "html.h"
#include "js.h"

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

const uint8_t ROW_TITLE = 0;
const uint8_t ROW_NETWORK = 1;
const uint8_t ROW_FILE = 2;
const uint8_t ROW_UPTIME = 3;
const uint8_t ROW_CURRENT = 4;
const uint8_t ROW_ERROR = 5;
const uint8_t ROW_OTHER = 5;

void setup() {
  Serial.begin(115200);
  initDisplay();
  initCpu();
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
    writeToDisplay("Failed to find INA219", ROW_CURRENT);
    while (1) {
      delay(10);
    }
  }
  // ina219.setCalibration_32V_1A();
}

void initCpu() {
  if (isLowPower) {
    setCpuFrequencyMhz(40);
    writeToDisplay("MilliApp - Low Power", ROW_TITLE);
  } else {
    setCpuFrequencyMhz(240);
    writeToDisplay("MilliApp", ROW_TITLE);
  }
}

void initNetwork() {
  if (isLowPower) {
    server.stop();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    writeToDisplay("Wifi off.", ROW_NETWORK);
  } else {
    WiFi.mode(WIFI_STA);
    writeToDisplay("Connecting...", ROW_NETWORK);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      writeToDisplay("Connection Failed!", ROW_NETWORK);
      return;
    }
    delay(5000);
    writeToDisplay("IP: " + ipToString(WiFi.localIP()), ROW_NETWORK);
    server.on("/", handle_OnConnect);
    server.on("/clearlog", handle_OnClearLog);
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
    writeToDisplay("SPIFFS Error", ROW_FILE);
    return;
  }
}

void openLogFile() {
  file = SPIFFS.open(logFileName, "a+");
  if (!file) {
    writeToDisplay("Unable to open log file", ROW_FILE);
    return;
  }
  if (!file.print("|")) {
    writeToDisplay("File error", ROW_FILE);
  } else {
    writeToDisplay("File: " + logFileName, ROW_FILE);
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
}

void writeToDisplay(String message) {
  writeToDisplay(message, ROW_OTHER);
}

void writeToDisplay(String message, int row) {
  display.setCursor(0, 10 * row);
  display.print(message);
  display.display();
}

void handle_OnClearLog() {
  bool formatted = SPIFFS.format();
  if (!formatted) {
    server.send(200, "text/html", "Error !" );
  } else {
    server.send(200, "text/html", "Done ! <A href='/'>Back</A>" );
  }

}

void handle_OnConnect() {
  File logFile = SPIFFS.open(logFileName);
  if (!logFile) {
    server.send(200, "text/html", "File error" );
    return;
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent(html);
  server.sendContent("<script>var data='");
  while (logFile.available()) {
    server.sendContent(logFile.readString());
  }
  server.sendContent("';" + js +  "</script>");
  server.client().stop();
}

void handleEvent(AceButton* , uint8_t eventType, uint8_t ) {
  switch (eventType) {
    case AceButton::kEventPressed:
      display.clearDisplay();
      isLowPower = !isLowPower;
      initCpu();
      initNetwork();
      if (isLowPower) {
        openLogFile();
        writeToDisplay("Low power mode!", ROW_OTHER) ;
      } else {
        file.close();
      }
      break;
  }
}


void handleINA219(String uptimeSeconds) {
  float current_mA = round(ina219.getCurrent_mA());

  if (isLowPower && current_mA >= 0 && current_mA != lastCurrent_mA) {
    if (!file.print( uptimeSeconds + ':' + String(current_mA) + ',')) {
      writeToDisplay("File error", ROW_FILE);
    } else {
      writeToDisplay("Data updated", ROW_FILE);
    }
    lastCurrent_mA = current_mA;
  }
  writeToDisplay("Current: " + String(current_mA) + "mA", ROW_CURRENT);

}


void loop() {
  uint32_t now = millis() ;
  String uptimeSeconds = String(now / 1000);
  if (now - lasttime >= lowPowerDelaySeconds * 1000 || !isLowPower) {
    lasttime = now;
    writeToDisplay("Uptime: " + uptimeSeconds, ROW_UPTIME );
    handleINA219(uptimeSeconds);
  }

  button.check();
  if (!isLowPower) {
    server.handleClient();
    ArduinoOTA.handle();
  }
}
