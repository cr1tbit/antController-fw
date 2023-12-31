// Ant controller firmware
// (C) cr1tbit 2023

#include <fmt/ranges.h>

#include <Wire.h>
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "LittleFS.h"
#include <SPIFFSEditor.h>
#include <WiFiSettings.h>
#include "ArduinoJson.h"

#include "commonFwUtils.h"
#include "alfalog.h"
#include "advancedOledLogger.h"

#include "pinDefs.h"
#include "antControllerHelpers.h"

#include "ioController.h"
#include "main.h"
#include "configHandler.h"

const char CONFIG_FILE[] = "/buttons.conf";

AsyncWebServer server(80);
AsyncEventSource events("/events");

IoController ioController;

bool shouldPostSocketUpdate = false;
volatile bool isrPinsChangedFlag = false;

SemaphoreHandle_t apiCallSemaphore;

DynamicJsonDocument getErrorJson(const std::string& msg){
    DynamicJsonDocument json(1024);
    json["msg"] = msg;
    json["retCode"] = 500;
    return json;
}
/* JSON schema:
 * {
    msg: string
    retCode: int
 */
DynamicJsonDocument mainHandleApiCall(const std::string &subpath, int* ret_code){
    ALOGD("Analyzing subpath: '{}'", subpath.c_str());
    //schema is <CMD>/<INDEX>/<VALUE>
    *ret_code = 200;

    auto api_split = splitString(subpath, '/');

    if (api_split.size() == 0){
        return getErrorJson("invnalid API call: " + subpath);
    }

    if( apiCallSemaphore == NULL ) {
        *ret_code = 500;
        return getErrorJson("API call mutex does not exist.");
    }
    if( xSemaphoreTake(apiCallSemaphore, (TickType_t)100) == pdTRUE) {
        DynamicJsonDocument json = ioController.handleApiCall(api_split);
        shouldPostSocketUpdate = true;
        xSemaphoreGive(apiCallSemaphore);
        return json;
    } else {
        *ret_code = 500;
        return getErrorJson("Timeout waiting for API call mutex.");
    }
}

bool initializeLittleFS(){
    if(!LittleFS.begin(false)){
        ALOGE("An Error has occurred while mounting SPIFFS");
        ALOGE("Is filesystem properly generated and uploaded?");
        ALOGE("(Note, in debug build filesystem does not autoformate)");
        return false;
    } else {
        ALOGD("LittleFS init ok.");
        ALOGD_RAW(
            "Using {}/{} kb.",
            LittleFS.usedBytes()/1024,
            LittleFS.totalBytes()/1024
        );

        ALOGD_RAW("Files:")
        listDir(LittleFS, "/");
        return true;
    }
}

void initializeHttpServer(){
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
        ALOGD("GET config");
        request->send(LittleFS, CONFIG_FILE, "text/plain", false);
    });

    server.on("/api", HTTP_GET, [](AsyncWebServerRequest *request){
        int ret_code = 418;

        std::string apiTrimmed = std::string(request->url().c_str()).substr(5);
        DynamicJsonDocument api_result = mainHandleApiCall(apiTrimmed, &ret_code);

        ALOGD("API call result:\n{}\n",api_result.as<String>());
        request->send(ret_code, "application/json", api_result.as<String>());
    });

    server.addHandler(new SPIFFSEditor(LittleFS, "test","test"));

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if (LittleFS.exists("/static/index.html")){
            ALOGD("GET index");
            request->send(LittleFS, "/static/index.html", "text/html", false);
        } else {
            request->send(404, "text/plain",
                "Site not found. "
                "The filesystem might've been created incorrectly. " 
                "See boot logs (via UART) for more information."
            );
        }        
    });

    server.serveStatic("/", LittleFS, "/static/");

    events.onConnect([](AsyncEventSourceClient *client){
        if(client->lastId()){
            ALOGD("Client reconnected! Last message ID that it had: {}\n", client->lastId());
        }
        client->send("AntController connected",NULL,millis(),1000);
        events.send(
            fmt::format("{}{}{}", 
                alogColorGrey, alogGetInitString(1), alogColorReset).c_str(),
            "log", millis());
        events.send(fmt::format("{}{}{}", 
                alogColorGrey, alogGetInitString(2), alogColorReset).c_str(),
            "log", millis());
        ALOGT("events connected");
        shouldPostSocketUpdate = true;
    });

    server.addHandler(&events);
    shouldPostSocketUpdate = true;

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    server.begin();
}

TwoWire i2c = TwoWire(0);
SerialLogger serialLogger = SerialLogger(
    [](const char* str) {Serial.println(str);},
    LOG_DEBUG, ALOG_FANCY, ALOG_FILELINE
);
SerialLogger socketLogger = SerialLogger(
    [](const char* str) { events.send(str,"log",millis());},
    LOG_INFO, ALOG_FANCY, ALOG_NOFILELINE
);
AdvancedOledLogger aOledLogger = AdvancedOledLogger(
    i2c, LOG_INFO, OLED_128x64, OLED_NORMAL);

void setup(){
    // #ifdef WAIT_FOR_SERIAL
    //         delay(2000);
    // #endif
    Serial.begin(115200);
    Serial.println(alogGetInitString());

    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS,HIGH);
    pinMode(PIN_BOOT_BUT1, INPUT);
    pinMode(PIN_BUT2, INPUT);
    pinMode(PIN_BUT3, INPUT);
    pinMode(PIN_BUT4, INPUT);

    i2c.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    aOledLogger.setTopBarText(
        BAR_WIFI_IP, "AntController r. " FW_REV);

    AlfaLogger.addBackend(&aOledLogger);
    AlfaLogger.addBackend(&serialLogger);
    AlfaLogger.addBackend(&socketLogger);

    AlfaLogger.begin();
    ALOGD("logger started");
    ALOG_I2CLS(i2c);

    ioController.begin(i2c);
    ALOGD("ioController start");

    if (initializeLittleFS()){
        ALOGT("LittleFS init ok.");
        // spawn on another task because main arduino task
        // has hardcoded 8kb stack size
        xTaskCreate( TomlTask, "toml task",
                65536, NULL, 6, NULL );
    } else { /*idk*/ }

    apiCallSemaphore = xSemaphoreCreateMutex();
    xTaskCreate( SerialTerminalTask, "serial task",
                10000, NULL, 2, NULL );
    ALOGI("Connecting WiFi...");
    WiFiSettings.onWaitLoop = []() {
        aOledLogger.redraw();
        return 100;
    };
    WiFiSettings.connect();//will require board reboot after setup
    ALOGI("IP: {}",WiFi.localIP());
    initializeHttpServer();

    ALOGI("Application start!");
}

int counter = 0;
void loop()
{
    int ret_code;
    handle_io_pattern(PIN_LED_STATUS, PATTERN_HBEAT);

    aOledLogger.redraw();

    delay(100);
    counter++;
    if (digitalRead(PIN_BUT4) == LOW){
        ALOGI("Button 4 is pressed - doing test call");
        ALOGI("Test call result: {0}",
            mainHandleApiCall(
                "REL/bits/"+std::to_string(counter++), &ret_code
            ).as<std::string>()
        );
    }

    if (counter%20 == 0){
        events.send(".","heartbeat",millis());
    }

    if (counter%100 == 0){
        // socketAlogHandle(fmt::format("heartbeat - runtime: {}s", millis()/1000).c_str());
        shouldPostSocketUpdate = true;
    }

    if (isrPinsChangedFlag){
        isrPinsChangedFlag = false;
        ALOGV("ISR pins changed");
        // shouldPostSocketUpdate = true;
    }

    if (shouldPostSocketUpdate){
        ALOGT("updating state by socket");
        shouldPostSocketUpdate = false;
        std::string ret = (ioController.getIoControllerState()).as<std::string>();
        events.send(ret.c_str(),"state",millis());
    }

}

void TomlTask(void *parameter)
{
    vTaskDelay(50 / portTICK_PERIOD_MS);
    while (1)
    {
        if (Config.loadConfig(CONFIG_FILE)){
            Config.printConfig();
            ALOGI("TomlTask done. Connecting to WiFi...");
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
                    mainHandleApiCall(
                        cmd, &ret_code)
                        .as<std::string>());
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