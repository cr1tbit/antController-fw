#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H

#include <map>

#ifdef ESP32
#include "alfalog.h"
#include "LittleFS.h"
#endif

#include "toml.hpp"

#include "ioControllerTypes.h"
#include <fmt/ranges.h>

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
            // ALOGHD(istr.str().c_str(), istr.str().length());

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
                pin_t pin (
                    toml::find<std::string>(v,"name"),
                    toml::find<std::string>(v,"antctrl"),
                    toml::find<std::string>(v,"sch")
                );
                pins.push_back(pin);
            }

            const auto _button_groups = toml::find(data, "buttons");

            for(const auto& bg : _button_groups.as_table()){
                button_groups[bg.first] = {};
                for(const auto& b : bg.second.as_array()){
                    button_t button(
                        toml::find<std::string>(b,"name"),
                        toml::find<std::vector<std::string>>(b,"pins")
                    );
                    button_groups[bg.first].buttons.push_back(button);
                }
            }
            assignPinsToButtonGroup();
            is_valid = true;
            printConfig();
            return true; 
        } catch (std::exception& e){
            ALOGE("Error parsing toml");
            ALOGE(e.what());
            return false;
        }
    }

    void assignPinsToButtonGroup() {
        // iterate over groups [a;d]
        for (auto& b_group : button_groups) {
            auto& groupName = b_group.first;
            auto& groupButtons = b_group.second;

            auto& groupPins = pins_by_group[groupName];
            // get a list of pointers to pins used in each group
            for(auto& b : groupButtons.buttons){
                for(auto& p : b.pinNames){
                    const pin_t& pin = getPinByName(p, false);
                    const pin_t* pinPtr = &pin;
                    // ALOGI("pin {}@{} assigned to group {}", 
                    //     pin.to_string(), 
                    //     static_cast<const void*>(pinPtr), 
                    //     groupName.c_str()
                    // );
                    groupPins.push_back(pinPtr);
                }
            }
            // remove repeating pin pointers
            std::sort(groupPins.begin(), groupPins.end());
            auto newEnd = std::unique(groupPins.begin(), groupPins.end());
            groupPins.erase(newEnd, groupPins.end());
            
            // now print all pointers attached to a group
            ALOGI("{} pins for group {}:",groupPins.size(), groupName.c_str());
            // ALOGI("{}", groupPins.at(0)->to_string());
            for (auto p : groupPins){
                ALOGI("{}", p->to_string());
            }
        }
    }

    const pin_t& getPinByName(const std::string& name, bool assertDuplicates = false){
        std::vector<pin_t> found_pins;
        for(auto& p : pins){
            // ALOGD(p.to_string());
            if ((p.name == name) || (p.sch == name)){
                if (!assertDuplicates){
                    return p;
                }
                found_pins.push_back(p);
            }
        }

        if (found_pins.size() == 1){
            return found_pins[0];
        } else if (found_pins.size() > 1){
            const std::string err = fmt::format(
                "pin {} is not unique!", name);
            throw std::runtime_error(err);
        } else {
            const std::string err = fmt::format(
            "pin {} not found!", name);
            throw std::runtime_error(err);
        }
    }

    void printConfig(){
        if (!is_valid){
            ALOGE("Invalid config!");
        }
        ALOGI("button map:")
        for (auto& b_group : button_groups) {
            ALOGI(b_group.first)
            for(auto& b : b_group.second.buttons){
                ALOGI(b.to_string())
            }
        }
        ALOGI("pins:")
        for(auto& p : pins){
            ALOGI(p.to_string())
        }
    }

    bool is_valid = false;

    std::map<std::string, buttonGroup_t> button_groups;
    std::map<std::string, std::vector<const pin_t*>> pins_by_group;
    std::vector<pin_t> pins;
};

extern Config_ &Config;

// once we move to espidf this will work. 
// void getStackUsed(){
//     TaskHandle_t xHandle;
//     TaskStatus_t xTaskDetails;

//     xHandle = xTaskGetHandle( "toml task" );
//     configASSERT( xHandle );

//     vTaskGetInfo(xHandle, &xTaskDetails, pdTRUE, eInvalid );

//     ALOGV("watermark is {}",xTaskDetails.usStackHighWaterMark);
// }

#endif //CONFIG_HANDLER_H