
#include "buttonHandler.h"

#include "configHandler.h"
#include "alfalog.h"

#include "ioController.h"

void addUniquePin(std::vector<pin_t>& pins, const pin_t& newPin) {
    auto it = std::find_if(pins.begin(), pins.end(), [&newPin](const pin_t& pin) {
        return pin.ioType == newPin.ioType && pin.ioNum == newPin.ioNum;
    });

    if (it == pins.end()) {
        pins.push_back(newPin);
    }
}

void logPinNames(std::string pre, const std::vector<pin_t>& pins) {
    std::vector<std::string> pinNames;
    for (const auto& pin : pins) {
        pinNames.push_back(pin.name);
    }
    ALOGI("{}: {}",pre, fmt::join(pinNames, ", "));
}

void ButtonHandler::getState(DynamicJsonDocument& jsonRef){
    JsonObject buttonHandlerData = jsonRef.createNestedObject("buttons");
    // buttonHandlerData["status"] = "OK";
    JsonObject buttonJson = buttonHandlerData.createNestedObject("groups");

    for(auto& bGroup: Config.button_groups){
        buttonJson[bGroup.first] = bGroup.second.currentButtonName;
    }
    return;
}

void ButtonHandler::resetOutputsForButtonGroup(const std::string& bGroup){
    // for (auto pin: Config.pins_by_group[bGroup]){
    //     ioController->setOutput(pin->ioType, pin->ioNum, false);
    // }
    std::vector<pin_t> pinsToTurnOff;

    for (auto& button: Config.button_groups[bGroup].buttons){
        for (auto& pinName: button.pinNames){
            const pin_t pin = Config.getPinByName(pinName);
            addUniquePin(pinsToTurnOff, pin);
        }
    }
    logPinNames("turning off pins", pinsToTurnOff);
    for (auto& pin: pinsToTurnOff){
        ioController->setOutput(pin.ioType, pin.ioNum, false);
    }
    Config.button_groups[bGroup].currentButtonName = "OFF";
}

bool ButtonHandler::setButton(button_t button, bool targetState){
    for (auto& bg: Config.button_groups){
        for (auto& b: bg.second.buttons){
            if (b.name == button.name){
                if (targetState){
                    return activateButtonFromGroup(bg.first, b);
                } else {
                    resetOutputsForButtonGroup(bg.first);
                    return true;
                }
            }
        }
    }
    ALOGE("button {} not found", button.name.c_str());
    return false;
}

bool ButtonHandler::activateButtonFromGroup(const std::string& bGroupName, button_t button){
    ALOGI("activate button {} in group {}", button.name, bGroupName);
    resetOutputsForButtonGroup(bGroupName);

    //translate button pin names to pin objects for easier operation
    std::vector<pin_t> pinsToActivate;
    for (auto& pinName: button.pinNames){
        const pin_t pin = Config.getPinByName(pinName);
        addUniquePin(pinsToActivate, pin);
    }

    // assert buttons guarded by to-be enabled pins
    for (auto& pin: pinsToActivate){
        for (auto& butName: pin.guardedButtonNames){
            if (butName.guardedButton == button.name){
                ALOGW("button {} is prevented by pin '{}' value!",
                    butName.guardedButton, pin.name)
                return false;
            } else {
                setButton(Config.getButtonByName(butName.guardedButton),false);
            }
        }
    }
    //then activate the pins
    for (auto& pin: pinsToActivate){
        this->ioController->setOutput(pin.ioType, pin.ioNum, true);
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
        ALOGT("checking button '{}'", b.name.c_str());
        if (b.name == buttonName){
            return activateButtonFromGroup(buttonGroup, b);
        }
    }
    ALOGI("button {} not found", api_call[1].c_str());
    return false;
}