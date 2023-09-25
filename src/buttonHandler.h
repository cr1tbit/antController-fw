
#pragma once


#include <vector>
#include <string>
#include "ioControllerTypes.h"
#include "ArduinoJson.h"

class IoController;

class ButtonHandler {

    IoController* dad;
public:
    ButtonHandler() = delete;
    ButtonHandler(IoController* dad){
        this->dad = dad;
    }

    bool apiAction(std::vector<std::string>& api_call);
    void resetOutputsForButtonGroup(const std::string& bGroup);
    bool activate_button(const std::string& bGroup, button_t button);
    void getState(DynamicJsonDocument& jsonRef);

private:
    std::string tag;
    std::vector<uint8_t> pins;
};