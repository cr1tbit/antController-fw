// Ant controller firmware
// (C) cr1tbit 2023

#include <fmt/ranges.h>

#include <Wire.h>
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "LittleFS.h"
#include <SPIFFSEditor.h>
#include <WiFiSettings.h>

#include "commonFwUtils.h"
#include "alfalog.h"
#include "advancedOledLogger.h"

#include "pinDefs.h"
#include "secrets.h"

#include "ioController.h"
#include "main.h"
#include "configHandler.h"

#define FW_REV "0.10.0"
const int WIFI_TIMEOUT_SEC = 15;
const char CONFIG_FILE[] = "/buttons.conf";

AsyncWebServer server(80);
AsyncEventSource events("/events");


IoController ioController;

std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::string::size_type start = 0;
    std::string::size_type end = str.find(delimiter);

    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }

    result.push_back(str.substr(start));

    return result;
}

std::string handleApiCall(const std::string &subpath, int* ret_code){
    //TODO add mutex due to serial/http conflict
    ALOGD("Analyzing subpath: {}", subpath.c_str());
    //schema is <CMD>/<INDEX>/<VALUE>
    *ret_code = 200;

    auto api_split = splitString(subpath, '/');

    if (api_split.size() == 0){
        return "ivnalid API call: " + subpath;
    }

    return ioController.handleApiCall(api_split);
}

bool initializeLittleFS(){
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

void initializeHttpServer(){
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, CONFIG_FILE, "text/plain", false);
    });

    server.on("/api", HTTP_GET, [](AsyncWebServerRequest *request){
        int ret_code = 418;

        std::string apiTrimmed = std::string(request->url().c_str()).substr(5);
        std::string api_result = handleApiCall(apiTrimmed, &ret_code);

        ALOGD("API call result:\n%s\n",api_result.c_str());
        request->send(ret_code, "text/plain", api_result.c_str());
    });

    server.addHandler(new SPIFFSEditor(LittleFS, "test","test"));

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/static/index.html", "text/html", false);
    });

    server.serveStatic("/", LittleFS, "/static/");

    events.onConnect([](AsyncEventSourceClient *client){
        if(client->lastId()){
                ALOGD("Client reconnected! Last message ID that it had: %u\n", client->lastId());
        }
        // send event with message "hello!", id current millis
        // and set reconnect delay to 1 second
        client->send("hello!",NULL,millis(),1000);
    });
    server.addHandler(&events);

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
SerialLogger serialLogger = SerialLogger(
    uartPrintAlogHandle, LOG_DEBUG, ALOG_FANCY);

SerialLogger socketLogger = SerialLogger(
    socketAlogHandle, LOG_DEBUG);

AdvancedOledLogger aOledLogger = AdvancedOledLogger(
    i2c, LOG_INFO, OLED_128x64, OLED_NORMAL);

void setup(){
    // #ifdef WAIT_FOR_SERIAL
    //         delay(2000);
    // #endif
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

    aOledLogger.setTopBarText(
        BAR_WIFI_IP, "AntController r. {}" FW_REV);

    AlfaLogger.addBackend(&aOledLogger);
    AlfaLogger.addBackend(&serialLogger);
    AlfaLogger.begin();
    ALOGD("logger started");

    ALOGI(
        "i2c device(s) found at:\n0x{:02x}",
        fmt::join(scan_i2c(i2c), ", 0x"));

    ioController.begin(i2c);
    ALOGD("ioController start");

    if (initializeLittleFS()){
        ALOGT("LittleFS init ok.");
        // spawn on another task because main arduino task
        // has hardcoded 8kb stack size
        xTaskCreate( TomlTask, "toml task",
                65536, NULL, 6, NULL );
    } else {
        // idk
    }

    #ifdef USE_SECRETS_H
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
        ALOGI("IP: {}",WiFi.localIP());
        initializeHttpServer();
    } else {
        ALOGE("WiFi timeout after {}s. "
        "The board will start in offline mode. "
        "API is still accesible via serial port.",
        WIFI_TIMEOUT_SEC);
    }
    #else
    WiFiSettings.connect();//will require board reboot after setup
    initializeHttpServer();
    #endif

    xTaskCreate( SerialTerminalTask, "serial task",
                10000, NULL, 2, NULL );

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
            handleApiCall(
                "REL/bits/"+std::to_string(counter++), &ret_code
            ).c_str()
        );
    }
}

void TomlTask(void *parameter)
{
    while (1)
    {
        if (Config.loadConfig(CONFIG_FILE)){
            ALOGV("Config loaded");
            Config.printConfig();
        } else {
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            ALOGE("Config load failed");
        }

        while (1)
            vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}

void SerialTerminalTask(void *parameter)
{
    std::vector<char> buf;

    int ret_code = 0;
    while (1){
        while (Serial.available())
        {
            char c = Serial.read();

            switch (c)
            {
            case '\r':
                Serial.print('\r');
                break;
            case '\b':
                if (buf.size() > 0)
                {
                    buf.pop_back();
                    Serial.print("\b \b");
                }
                break;
            case '\n':
            {
                const std::string cmd(buf.begin(), buf.end());
                ALOGV("Op result: {}",
                        handleApiCall(
                            cmd, &ret_code)
                            .c_str());
                buf.clear();
                break;
            }
            default:
            {
                if (buf.size() <= 40)
                {
                    buf.push_back(c);
                    Serial.print(c);
                }
            }
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}