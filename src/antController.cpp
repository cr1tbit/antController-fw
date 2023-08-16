// Ant controller firmware
// (C) cr1tbit 2023

#include <fmt/ranges.h>

#include <Wire.h>
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "LittleFS.h"
#include <SPIFFSEditor.h>

#include "commonFwUtils.h"
#include "alfalog.h"
#include "advancedOledLogger.h"

#include "pinDefs.h"
#include "secrets.h"

#include "ioController.h"
#include "antController.h"
#include "configHandler.h"

#define FW_REV "0.8.0"
const int WIFI_TIMEOUT_SEC = 15;

AsyncWebServer server(80);
AsyncEventSource events("/events");

String handle_api_call(const String &subpath, int* ret_code){
  ALOGD("Analyzing subpath: {}", subpath.c_str());
  //schema is <CMD>/<INDEX>/<VALUE>

  *ret_code = 200; 

  output_group_type_t output;

  std::vector<String>api_split;
  int delimiterIndex = 0;
  int previousDelimiterIndex = 0;
  
  while ((delimiterIndex = subpath.indexOf("/", previousDelimiterIndex)) != -1) {
    api_split.push_back(subpath.substring(previousDelimiterIndex, delimiterIndex));
    previousDelimiterIndex = delimiterIndex + 1;
  }  
  api_split.push_back(subpath.substring(previousDelimiterIndex));
  //now api_split contains all the parts of the path

  if (api_split.size() == 0){
    return "ivnalid API call: " + subpath;
  }  

  return IoController.handleApiCall(api_split);
}

bool initialize_littleFS(){
  if(!LittleFS.begin(false)){
    ALOGE("An Error has occurred while mounting SPIFFS");
    ALOGE("Is filesystem properly generated and uploaded?");
    ALOGE("(Note, in debug build filesystem does not autoformate)");
    return false;
  } else {
    ALOGD("LittleFS init ok.");
    ALOGD(
      "Using {}/{} kb.",
      LittleFS.usedBytes()/1024,
      LittleFS.totalBytes()/1024
    );
    
    ALOGD("Files:")
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while(file){
      ALOGD("{}: {}b",
        file.name() ,file.size());
      file = root.openNextFile();
    }
    return true;
  }
}

void initialize_http_server(){

  server.on("/api", HTTP_GET, [](AsyncWebServerRequest *request){
    int ret_code = 418;
    String api_result = handle_api_call(request->url().substring(5),&ret_code);
    ALOGD("API call result:\n%s\n",api_result.c_str());
    request->send(ret_code, "text/plain", api_result);
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", String(), false);
  });
  server.serveStatic("/", LittleFS, "/");

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      ALOGD("Client reconnected! Last message ID that it had: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!",NULL,millis(),1000);
  });
  server.addHandler(&events);
  server.addHandler(new SPIFFSEditor(LittleFS, "test","test"));
  
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
}

void uartPrintAlogHandle(const char* str){
  Serial.println(str);
}

void socketAlogHandle(const char* str){
  events.send(str,"log",millis());
}

TwoWire i2c = TwoWire(0);
SerialLogger serialLogger = SerialLogger(uartPrintAlogHandle, LOG_DEBUG, ALOG_FANCY);
SerialLogger socketLogger = SerialLogger(socketAlogHandle, LOG_DEBUG);
AdvancedOledLogger display = AdvancedOledLogger(i2c, OLED_128x32, LOG_INFO);

void setup(){
  Serial.begin(115200);
  Serial.println("============================");
  Serial.println("AntController fw rev. " FW_REV);
  Serial.println("Compiled " __DATE__ " " __TIME__);

  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS,HIGH);
  pinMode(PIN_BOOT_BUT1, INPUT);
  pinMode(PIN_BUT2, INPUT);
  pinMode(PIN_BUT3, INPUT);
  pinMode(PIN_BUT4, INPUT);

  i2c.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  display.staticTopBarText = fmt::format(
    "AntController r. {}", FW_REV); 

  AlfaLogger.addBackend(&display);
  AlfaLogger.addBackend(&serialLogger);
  AlfaLogger.begin();
  ALOGD("logger started");

  ALOGI(
    "i2c device(s) found at:\n0x{:02x}", 
    fmt::join(scan_i2c(i2c), ", 0x"));

  IoController.begin(i2c);
  ALOGD("IoController start");

  if (initialize_littleFS()){
    ALOGT("LittleFS init ok.");
    xTaskCreate( TomlTask, "toml task",
      65536, NULL, 6, NULL );
  } else {
    // idk
  }

  long conn_attempt_start = millis();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(3000);
    ALOGI("Connecting WiFi...");
    if (millis() - conn_attempt_start > WIFI_TIMEOUT_SEC*1000){
      break;
    }
  }
  if (WiFi.status() == WL_CONNECTED){
    ALOGI("IP: {}",WiFi.localIP().toString().c_str());
    initialize_http_server();
  } else {
    ALOGE("WiFi timeout after {}s. "
    "The board will start in offline mode. "
    "API is still accesible via serial port.",
    WIFI_TIMEOUT_SEC);
  }

  xTaskCreate( SerialTerminalTask, "serial task",
    6000, NULL, 2, NULL );
  
  ALOGI("Application start!");
}

int counter = 0;
void loop()
{
  int ret_code;
  handle_io_pattern(PIN_LED_STATUS, PATTERN_HBEAT);
  
  delay(300);
  if (digitalRead(PIN_BUT4) == LOW){
    ALOGI("Button 4 is pressed - doing test call");
    ALOGI("Test call result: {0}",
      handle_api_call(
        "REL/bits/"+String(counter++), &ret_code
      ).c_str()
    );
  }
}

const char CONFIG_FILE[] = "/buttons.conf";

void TomlTask (void * parameter){
  while(1){
    Config.loadConfig(CONFIG_FILE);
    ALOGV("Config loaded");
    Config.printConfig();
    while(1) vTaskDelay(60000/portTICK_PERIOD_MS);
  }
}

void SerialTerminalTask( void * parameter ) {
  std::vector<char> buf;

  int ret_code = 0;
  while(1){
    while (Serial.available()) {
      char c = Serial.read();
      
      switch (c){
        case '\r': 
          Serial.print('\r');
          break;
        case '\b':
          if(buf.size() > 0){
            buf.pop_back();
            Serial.print("\b \b");
          }
          break;
        case '\n': {
          const std::string cmd(buf.begin(), buf.end());
          ALOGV("Op result: {}",
            handle_api_call(
              String(cmd.c_str()), &ret_code
            ).c_str()
          );
          buf.clear();
          break;
        }
        default: {
          if(buf.size() <= 40){
            buf.push_back(c);
            Serial.print(c);
          }
        }
      }
    }
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
}