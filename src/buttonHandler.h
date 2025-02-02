
#pragma once


#include <vector>
#include <string>
#include "ioControllerTypes.h"
#include "ArduinoJson.h"

class IoController;

class ButtonHandler {

    IoController* ioController;
public:
    ButtonHandler() = delete;
    ButtonHandler(IoController* ioController){
        this->ioController = ioController;
    }

    bool apiAction(std::vector<std::string>& api_call);
    void resetOutputsForButtonGroup(const std::string& bGroup);
    bool activateButtonFromGroup(const std::string& bGroup, button_t button);
    bool setButton(button_t button, bool targetState);
    bool getButton(button_t button, bool* gottenState);
    void getState(DynamicJsonDocument& jsonRef);
    bool recheckPinGuards(bool inputOnly = false);

private:
    std::string tag;
    std::vector<uint8_t> pins;
};