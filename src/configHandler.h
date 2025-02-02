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
            const auto version = toml::find<std::string>(data, "version");
            ALOGD("Config version: {}", version);

            int statPinCount = parsePins(data);            
            int statButtonCount = parseButtons(data);

            // int statCondCount = parseCondRules(data);            
            // assignPinsToButtonGroup();

            is_valid = true;
            ALOGI("Loaded {} pins, {} buttons",
                statPinCount, statButtonCount);
            // printConfig();
            return true;
        } catch (std::exception& e){
            ALOGE("Error parsing toml");
            ALOGE(e.what());
            return false;
        }
    }

    int parsePins(toml::value& v){
        const auto values = toml::find(v, "pin");
        int counter = 0;

        for(const auto& v : values.as_array()) {
            pin_t pin (
                toml::find<std::string>(v,"name"),
                toml::find<std::string>(v,"antctrl"),
                toml::find<std::string>(v,"sch")
            );
            pins.push_back(pin);
            counter++;
        }
        return counter;
    }

    int parseButtons(toml::value& v){
        const auto _button_groups = toml::find(v, "buttons");
        int counter = 0;

        for(const auto& bg : _button_groups.as_table()){
            button_groups[bg.first] = {};
            for(const auto& b : bg.second.as_array()){
                button_t button(
                    toml::find<std::string>(b,"name"),
                    toml::find<std::vector<std::string>>(b,"pins")
                );
                if (b.contains("disable_on_low")){
                    const std::vector<std::string>& condPinNames = 
                        toml::find<std::vector<std::string>>(b,"disable_on_low");

                    for (auto& p : condPinNames){
                        pin_t& pin = getPinByName(p);
                        pin.setGuard(button.name, false);
                    }
                }
                if (b.contains("disable_on_high")){
                    const std::vector<std::string>& condPinNames = 
                        toml::find<std::vector<std::string>>(b,"disable_on_high");

                    for (auto& p : condPinNames){
                        pin_t& pin = getPinByName(p);
                        pin.setGuard(button.name, true);
                    }
                }
                counter++;
                button_groups[bg.first].buttons.push_back(button);
            }
        }
        return counter;
    }

    pin_t& getPinByName(const std::string& name, bool assertDuplicates = false){
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
            "pin '{}' not found!", name);
            throw std::runtime_error(err);
        }
    }


    std::vector<std::tuple<pin_t&, pinGuard_t&>> gatherGuards( bool inputOnly = false, 
        std::string forPinName = "", std::string forButtonName = ""
    ){
        std::vector<std::tuple<pin_t&, pinGuard_t&>> guards;

        for (auto& pin: pins){
            if (inputOnly && (pin.ioType != INP)){
                continue;
            }
            if ((forPinName != "") && (pin.name != forPinName)){
                continue;
            }
            for (auto& guard: pin.pinGuards){
                if ((forButtonName != "") && (guard.guardedButton != forButtonName)){
                    continue;
                }
                guards.push_back(std::forward_as_tuple(pin, guard));
                ALOGT("push pin |{}| guards button |{}|", pin.name, guard.guardedButton);
            }
        }
        ALOGT("gathered {} guards", guards.size());
        return guards;
    }

    button_t& getButtonByName(const std::string& name){
        for(auto& bg : button_groups){
            for(auto& b : bg.second.buttons){
                if (b.name == name){
                    return b;
                }
            }
        }
        const std::string err = fmt::format(
            "button '{}' not found!", name);
        throw std::runtime_error(err);
    }

    void printConfig(){
        if (!is_valid){
            ALOGE("Invalid config!");
        }
        ALOGD_RAW("== button map ==")
        for (auto& b_group : button_groups) {
            ALOGD_RAW("group {}:", b_group.first)
            for(auto& b : b_group.second.buttons){
                ALOGD_RAW("\t{}", b.to_string())
            }
        }
        ALOGD_RAW("== pins ==")
        for(auto& p : pins){
            ALOGD_RAW("\t{}", p.to_string())
        }
    }

    bool is_valid = false;

    std::map<std::string, buttonGroup_t> button_groups;
    // std::map<std::string, std::vector<const pin_t*>> pins_by_group;
    std::vector<pin_t> pins;
};

extern Config_ &Config;

#endif //CONFIG_HANDLER_H