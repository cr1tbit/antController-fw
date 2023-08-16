#pragma once

#include <map>


#ifdef ESP32
#include "alfalog.h"
#include "LittleFS.h"
#endif

#include "toml.hpp"

typedef struct {
    std::string name;
    std::string antctrl;
    std::string sch;

    std::string to_string(){
        return name + ": " + antctrl + "/" + sch;
    }
} pin_t;

typedef struct {
    std::string name;
    std::vector<std::string> pins;

    std::string to_string(){
        std::string ret = name + ": ";
        for(const auto& p : pins){
            ret += p + " ";
        }
        return ret;
    }
} button_t;

class Config_ {
public:
    Config_() = default;

    static Config_ &getInstance(){
        static Config_ instance;
        return instance;
    }

    #ifdef ESP32
    bool loadConfig(const char* name){
        File file = LittleFS.open(name, "r", false);

        if(!file.available()){
            ALOGD("config file not found");
            return false;
        } else {
            std::istringstream istr(file.readString().c_str());

            istr.seekg(0, std::ios::end);
            int size = istr.tellg();
            ALOGD("loaded {}B toml file", size);
            istr.seekg(0, std::ios::beg);

            return parseToml(istr, name);
        }  
    }
    #endif

    bool parseToml(std::istringstream& istr, const char* name){
        try {
            auto data = toml::parse(istr, name);
            const auto title = toml::find<std::string>(data, "version");

            ALOGI(title);

            const auto values = toml::find(data, "pin");        
            for(const auto& v : values.as_array()) {
                pin_t pin = {
                    .name = toml::find<std::string>(v,"name"),
                    .antctrl = toml::find<std::string>(v,"antctrl"),
                    .sch = toml::find<std::string>(v,"sch")
                };
                pins.push_back(pin);
            }

            const auto button_groups = toml::find(data, "buttons");

            for(const auto& bg : button_groups.as_table()){
                button_map[bg.first] = {};
                for(const auto& b : bg.second.as_array()){
                    button_t button = {
                        .name = toml::find<std::string>(b,"name"),
                        .pins = toml::find<std::vector<std::string>>(b,"pins")
                    };
                    button_map[bg.first].push_back(button);
                }
            }
            return true;
        } catch (std::exception& e){
            ALOGE("Error parsing toml");
            ALOGE(e.what());
            return false;
        }             
    }

    void printConfig(){
        ALOGI("button map:")
        for (auto& b_group : button_map) {
            ALOGI(b_group.first)
            for(auto& b : b_group.second){
                ALOGI(b.to_string())
            }
        }
        ALOGI("pins:")
        for(auto& p : pins){
            ALOGI(p.to_string())
        }
    }
    std::map<std::string, std::vector<button_t>> button_map;
    std::vector<pin_t> pins;
};


Config_ &Config = Config.getInstance();


    // once we move to espidf this will work. 
    // void getStackUsed(){
    //     TaskHandle_t xHandle;
    //     TaskStatus_t xTaskDetails;

    //     xHandle = xTaskGetHandle( "toml task" );
    //     configASSERT( xHandle );

    //     vTaskGetInfo(xHandle, &xTaskDetails, pdTRUE, eInvalid );

    //     ALOGV("watermark is {}",xTaskDetails.usStackHighWaterMark);
    // }