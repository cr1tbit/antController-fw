
#include "buttonHandler.h"

#include "configHandler.h"
#include "alfalog.h"

#include "ioController.h"

void ButtonHandler::resetOutputsForButtonGroup(const std::string& bGroup){
    for (auto pin: Config.pins_by_group[bGroup]){
        dad->setOutput(pin->ioType, pin->ioNum, false);
    }
} 

bool ButtonHandler::activate_button(const std::string& bGroup, button_t button){
    ALOGI("activate button {} in group {}", button.name, bGroup);
    resetOutputsForButtonGroup(bGroup);

    for (auto& pinName: button.pinNames){
        const pin_t pin = Config.getPinByName(pinName);
        dad->setOutput(pin.ioType, pin.ioNum, true);
    }
    return true;
}

bool ButtonHandler::api_action(std::vector<String>& api_call){
    ALOGI("API call for buttonHandler");

    if (api_call.size() < 2){
        return false;
    }

    for (auto& group: Config.button_groups){
        for (auto &b: group.second.buttons){
            // ALOGI("checking button {}", b.name.c_str());
            if (b.name == std::string(api_call[1].c_str())){
            return activate_button(group.first, b);
            }
        }
    }
    ALOGI("button {} not found", api_call[1].c_str());

    return false;
}