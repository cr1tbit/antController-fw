
#include "buttonHandler.h"

#include "configHandler.h"
#include "alfalog.h"

#include "ioController.h"

void ButtonHandler::getState(DynamicJsonDocument& jsonRef){
    JsonObject buttonHandlerData = jsonRef.createNestedObject("BUT");
    buttonHandlerData["status"] = "OK";
    JsonObject buttonJson = buttonHandlerData.createNestedObject("groups");

    for(auto& bGroup: Config.button_groups){
        buttonJson[bGroup.first] = bGroup.second.currentButtonName;        
    }
    return;
}

void ButtonHandler::resetOutputsForButtonGroup(const std::string& bGroup){
    // for (auto pin: Config.pins_by_group[bGroup]){
    //     dad->setOutput(pin->ioType, pin->ioNum, false);
    // }
    for (auto& button: Config.button_groups[bGroup].buttons){
        for (auto& pinName: button.pinNames){
            const pin_t pin = Config.getPinByName(pinName);
            dad->setOutput(pin.ioType, pin.ioNum, false);
        }
    }
    Config.button_groups[bGroup].currentButtonName = "OFF";
} 

bool ButtonHandler::activate_button(const std::string& bGroupName, button_t button){
    ALOGI("activate button {} in group {}", button.name, bGroupName);
    resetOutputsForButtonGroup(bGroupName);

    for (auto& pinName: button.pinNames){
        const pin_t pin = Config.getPinByName(pinName);
        dad->setOutput(pin.ioType, pin.ioNum, true);
    }
    Config.button_groups[bGroupName].currentButtonName = button.name;

    return true;
}

bool ButtonHandler::apiAction(std::vector<std::string>& api_call){
    ALOGI("API call for buttonHandler");

    if (api_call.size() < 3){
        return false;
    }
    std::string buttonGroup = api_call[1];
    std::string buttonName = api_call[2];

    if (Config.button_groups.count(buttonGroup) == 0){
        ALOGI("button group {} not found", buttonGroup);
        return false;
    }

    if (buttonName == "OFF"){
        resetOutputsForButtonGroup(buttonGroup);
        return true;
    }

    for (auto &b: Config.button_groups[buttonGroup].buttons){
        // ALOGI("checking button {}", b.name.c_str());
        if (b.name == buttonName){
            return activate_button(buttonGroup, b);
        }
    }
    ALOGI("button {} not found", api_call[1].c_str());
    return false;
}