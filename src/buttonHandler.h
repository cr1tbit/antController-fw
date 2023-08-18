
#pragma once


#include <vector>
#include <string>
#include "ioControllerTypes.h"

class IoController;

class ButtonHandler {

    IoController* dad;
  public:
    ButtonHandler() = delete;
    ButtonHandler(IoController* dad){
      this->dad = dad;
    }

    bool api_action(std::vector<String>& api_call);
    void resetOutputsForButtonGroup(const std::string& bGroup);
    bool activate_button(const std::string& bGroup, button_t button);

  private:
    String tag;
    std::vector<uint8_t> pins;
};