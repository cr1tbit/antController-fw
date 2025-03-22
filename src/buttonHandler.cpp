
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
    ALOGD("resetting outputs for group {}", bGroup.c_str());
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

bool ButtonHandler::getButton(button_t button, bool* gottenState){
    for (auto& bg: Config.button_groups){
        for (auto& b: bg.second.buttons){
            if (b.name == button.name){
                *gottenState = (bg.second.currentButtonName == button.name);
                return true;
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

    // works unreliably currently, disable for now

    // // assert buttons guarded by to-be enabled pins
    // for (auto& pin: pinsToActivate){
    //     for (auto& pinGuard: pin.pinGuards){
    //         ALOGI("Checking pin: '{}'", pin.to_string().c_str());
    //         if (pinGuard.guardedButton == button.name){
    //             ALOGW("button '{}' is prevented by pin '{}' value!",
    //                 pinGuard.guardedButton, pin.name)
    //             return false;
    //         } else {
    //             ALOGW("button '{}' is being turned off", pinGuard.guardedButton)
    //             setButton(Config.getButtonByName(pinGuard.guardedButton),false);
    //         }
    //     }
    // }
    //then activate the pins
    logPinNames("turning on pins", pinsToActivate);
    for (auto& pin: pinsToActivate){
        this->ioController->setOutput(pin.ioType, pin.ioNum, true);
    }
    Config.button_groups[bGroupName].currentButtonName = button.name;

    recheckPinGuards();
    return true;
}

bool ButtonHandler::recheckPinGuards(bool inputOnly){
    auto guards = Config.gatherGuards(inputOnly);

    for (auto& [pin, guard]: guards){
        // 1. Ignore output pins if needed
        if (inputOnly && pin.ioType != INP){
            continue;
        }
        // 2. Assert that this guard is activated
        bool pinValue = ioController->getIoValue(pin.ioType, pin.ioNum);
        if (pinValue != guard.onHigh){
            continue;
        }
        // 3. Check if the guarded button is active
        button_t currentButton = Config.getButtonByName(guard.guardedButton);

        bool isHigh;
        if (getButton(currentButton, &isHigh)){
            if (isHigh){
                ALOGW("pin |{}| is |{}|, guarding button |{}|, turn off",
                    pin.name, pinValue?"high":"low", guard.guardedButton);
                setButton(currentButton, false);
            }
        }
    }
    return true;
}

bool ButtonHandler::apiAction(std::vector<std::string>& api_call){
    ALOGT("API call for buttonHandler");

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




    // for (auto& pin: Config.pins){
    //     if (pin.ioType != INP){
    //         if (inputOnly) continue;
    //     }

    //     for (auto& guard: pin.pinGuards){
    //         if (ioController->getIoValue(pin.ioType, pin.ioNum) != guard.onHigh){
    //             continue;
    //         }
    //         button_t currentButton = Config.getButtonByName(guard.guardedButton);

    //         bool isHigh;
    //         if (getButton(currentButton, &isHigh)){
    //             if (isHigh){
    //                 ALOGW("pin {} is in wrong state for button {}, turn off",
    //                     pin.name, guard.guardedButton);
    //                 setButton(currentButton, false);
    //             }
    //         }
    //     }
    // }