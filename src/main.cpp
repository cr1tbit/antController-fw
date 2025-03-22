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

#include "gracefulRestart.h"
#include "clitussiStub.h"

const char CONFIG_FILE[] = "/buttons.conf";
const char CONFIG_FALLBACK[] = "/buttons_simple.conf";

static const char *config_filename = CONFIG_FILE;

AsyncWebServer server(80);
AsyncEventSource events("/events");

IoController ioController;

bool shouldPostSocketUpdate = false;
bool deviceHasValidState = true;

clitussiStub clitussi;

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

//create enum with potential sources of API calls
typedef enum {
    API_SOURCE_SOCKET,
    API_SOURCE_SERIAL,
    API_SOURCE_HTTP
} apiSource_t;

bool spurt(const String& fn, const std::string& content) {
    File f = LittleFS.open(fn, "w");
    if (!f) {
        return false;
    }
    auto w = f.print(content.c_str());
    f.close();
    return w == content.length();
}

DynamicJsonDocument mainHandleApiCall(const std::string &subpath, int* ret_code, apiSource_t source = API_SOURCE_HTTP){
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
    if(!LittleFS.begin(true)){
        // ALOGE("An Error has occurred while mounting SPIFFS");
        // ALOGE("Is filesystem properly generated and uploaded?");
        // ALOGE("(Note, in debug build filesystem does not autoformate)");
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
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "content-type");

    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
        ALOGD("GET config");
        request->send(LittleFS, config_filename, "text/plain", false);
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
    ioController.attachNotifyTaskHandle(xTaskGetCurrentTaskHandle());
    ALOGD("ioController start");

    if (initializeLittleFS()){
        ALOGT("LittleFS init ok.");

        if (digitalRead(PIN_BUT4) == LOW){
            ALOGI("Button 4 is pressed - the device will load a default config");
            config_filename = CONFIG_FALLBACK;
        }
    
        // spawn on another task because main arduino task
        // has hardcoded 8kb stack size
        bool taskCreated = xTaskCreate( TomlTask, "toml task",
                100*1000, (void*) config_filename, 6, NULL );
        if (taskCreated != pdPASS) {
            ALOGE("Failed to create TOML task");
            deviceHasValidState = false;    
        }
    } else { 
        deviceHasValidState = false;
    }

    vTaskDelay(3000 / portTICK_PERIOD_MS);

    apiCallSemaphore = xSemaphoreCreateMutex();
    xTaskCreate( SerialTerminalTask, "serial task",
                10000, NULL, 2, NULL );
    ALOGI("Connecting WiFi...");
    WiFiSettings.onWaitLoop = []() {
        aOledLogger.redraw();
        return 100;
    };
    WiFiSettings.onPortal = []() {
        ALOGE("Couldn't connect to WiFi. "
        "Connect to wifi beginning with \"esp\" with your smartphone. "
        "The OLED screen will now hang.");
        aOledLogger.redraw();
    };

    WiFiSettings.connect();//will require board reboot after setup
    ALOGI("IP: http://{}/",WiFi.localIP());
    initializeHttpServer();

    ALOGI("Application start!");
}

void apiTest(){
    static int counter = 0;
    int ret_code;

    if (digitalRead(PIN_BUT4) == LOW){
        ALOGI("Button 4 is pressed - doing test call");
        ALOGI("Test call result: {0}",
            mainHandleApiCall(
                "REL/bits/"+std::to_string(counter++), &ret_code
            ).as<std::string>()
        );
    }
}

int counter = 0;
void loop()
{
    if (ulTaskNotifyTake(pdTRUE, 100 / portTICK_PERIOD_MS)){
        ALOGT("Task notified");
        shouldPostSocketUpdate = true;
    }

    aOledLogger.redraw();
    counter++;
    apiTest();

    if (counter%20 == 0){
        events.send(".","heartbeat",millis());
    }

    if (counter%100 == 0){
        shouldPostSocketUpdate = true;
    }

    if (shouldPostSocketUpdate){
        ALOGT("updating state by socket");
        shouldPostSocketUpdate = false;
        std::string ret = (ioController.getIoControllerState()).as<std::string>();
        events.send(ret.c_str(),"state",millis());

        counter = 0;
    }
}

void TomlTask(void *parameter)
{
    const char* config = (const char*) parameter; 
    ALOGV("TOMl task start");
    vTaskDelay(50 / portTICK_PERIOD_MS);

    if (Config.loadConfig(config) == false){
        deviceHasValidState = false;
        ALOGE("Config load failed. Attempt fallback");
        config_filename = CONFIG_FALLBACK;
        if (Config.loadConfig(config_filename) == false){
            ALOGE("Fallback config load failed.");            
        } else {
            ALOGW("WARNING - Fallback config loaded.");
        }
    }

    Config.printConfig();
    ALOGI("TomlTask done. Connecting to WiFi...");
    vTaskDelete(NULL);
}

void SerialTerminalTask(void *parameter)
{   
    clitussi.attachCommandCb("wifi",[](std::string cmd){
        auto substrings = splitString(cmd, ' ');
        if (substrings.size() == 3){
            if (spurt("/wifi-ssid", substrings[1]) && spurt("/wifi-password", substrings[2])){
                ALOGI("New WIFI settings saved. Resetting the device");
                gracefulRestart();
            } else {
                ALOGE("Error writing to FS");
            }
        } else {
            ALOGE("Invalid WIFI command");
        }
    });

    clitussi.attachCommandCb("IMPROV",[](std::string cmd){
        //TODO: IMPROVe this section
        ALOGI("IMPROV command received");
    });

    clitussi.attachCommandCb("restart",[](std::string cmd){
        gracefulRestart();
    });

    clitussi.attachCommandCb("API",[](std::string cmd){
        int ret_code;

        ALOGV("Op result: {}",
            mainHandleApiCall(
                cmd, &ret_code, API_SOURCE_SERIAL)
                .as<std::string>(),ret_code);
    });

    for (;;){
        clitussi.loop();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}