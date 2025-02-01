#ifndef IO_CONTROLLER_H
#define IO_CONTROLLER_H

#include <vector>

#include <Arduino.h>
#undef B1
#include <Wire.h>
#include "ArduinoJson.h"

#include "ioControllerTypes.h"
#include "configHandler.h"
#include "pinDefs.h"

#include "buttonHandler.h"

const int EXP_MOS_ADDR = 0x20;
const int EXP_REL_ADDR = 0x21;
const int EXP_OPTO_ADDR = 0x22;


class IoGroup {

  protected:
    IoGroup(antControllerIoType_t ioType){
      this->ioType = ioType;
      this->tag = std::string(ioTypeMap.at(ioType));
    }

  public:

    bool tryParseApiArguments(std::vector<std::string>& api_call, DynamicJsonDocument& jsonRef){
        switch (api_call.size()){
        case 0:
        case 1:
          jsonRef["msg"] = "ERR: no parameter";
          jsonRef["retCode"] = 500;
          return false;
        case 2:
          jsonRef["parameter"] = api_call[1];
          jsonRef["value"] = "";
          return true;
        case 3:
          jsonRef["parameter"] = api_call[1];
          jsonRef["value"] = api_call[2];
          return true;
        default:
          jsonRef["msg"] = "ERR: too many parameters";
          jsonRef["retCode"] = 500;
          return false;
      }
    }

    DynamicJsonDocument apiAction(std::vector<std::string>& api_call){
      ALOGI("API call for tag {}",
        tag.c_str(), api_call.size());

      DynamicJsonDocument retJson(1024);

      if (!tryParseApiArguments(api_call, retJson)){
        //append args for debugging
        JsonArray args = retJson.createNestedArray("args");
        for (int i = 0; i<api_call.size(); i++){
            args[i] = api_call[i];
        }
        return retJson;
      } else {
        ioOperation(retJson);
        return retJson;
      }
    }

    int intFromString(std::string& str){
      if (str.length() == 0){
        return -1;
      }
      //check if std::string contains any non-digit characters
      for (int i = 0; i < str.length(); i++){
        if (!isDigit(str.at(i))){
          return -1;
        }
      }
      return std::stoi(str);
    }

    void appendJsonStatus(DynamicJsonDocument& jsonRef, bool isSucc, const char* msg){
      jsonRef["msg"] = msg;
      jsonRef["retCode"] = isSucc ? 200 : 500;
    }

    antControllerIoType_t ioType;
    std::string tag;
    virtual void resetOutputs() = 0;
    virtual void getState(JsonObject& jsonRef) = 0;
    virtual bool isPinHigh(int pin_num) = 0;

  private:
    virtual void ioOperation(DynamicJsonDocument& jsonRef) = 0;
};

class O_group : public IoGroup {
  public:
    O_group(antControllerIoType_t ioType, PCA9555* p_exp, int out_num, int out_offs)
     : IoGroup(ioType){
      this->out_num = out_num;
      this->out_offs = out_offs;

      this->expander = p_exp;
    }

    bool set_output(int pin_num, bool val){
      if (pin_num >= out_num){
        return false;
      }
      if (pin_num < 0){
        return false;
      }

      int offs_pin = pin_num + out_offs;

      ALOGT("set pin {} {} @ {}",
        offs_pin,
        val ? "on" : "off",
        tag.c_str()
      );
      expander->write(
        (PCA95x5::Port::Port) offs_pin,
        (PCA95x5::Level::Level )val
      );
      return true;
    }

    bool set_output_bits(uint16_t bits){
      int bit_range = pow(2,out_num);
      if (bits >= bit_range){
        // bits are out of range
        ALOGE("Cannot write bits {:#04x} as it exceeds {:#04x} on {}",
          bits, bit_range, tag.c_str());
        return false;
      }
      ALOGI("Write bits {:#04x} on {}",bits, tag.c_str());
      bits &= (0xFFFF >> 16-out_num);
      bits <<= out_offs;
      bits |= expander->read() & ~(0xFFFF >> 16-out_num << out_offs);
      expander->write(bits);
      return true;
    }

    uint16_t get_output_bits(){
      uint16_t bits = expander->read();
      bits >>= out_offs;
      bits &= (0xFFFF >> 16-out_num);
      return bits;
    }

    int tryGetPinByParameter(std::string& parameter){
      int parameter_int_offs = intFromString(parameter);
      if (parameter_int_offs > 0){
        parameter_int_offs -= 1; // API pin parameters start from 1
        if (parameter_int_offs >= out_num){
          return -1;
        }
        return parameter_int_offs;
      } else {
        return -1;
      }
    }

    bool isPinHigh(int pin_num){
      return expander->read(
        (PCA95x5::Port::Port) (pin_num + out_offs)
      );
    }

    bool tryWritePinByParam(int pinOffs, std::string& parameter){
      if (parameter.find("on") != std::string::npos) {
        return set_output(pinOffs, true);
      } else if (parameter.find("off") != std::string::npos) {
        return set_output(pinOffs, false);
      } else {
        return false;
      }
    }

    void ioOperation(DynamicJsonDocument& jsonRef){
      std::string parameter = jsonRef["parameter"].as<std::string>();
      std::string value = jsonRef["value"].as<std::string>();

      //try to find a pin based on integer or pin name
      int expPinByParam = tryGetPinByParameter(parameter);
      if (expPinByParam >= 0){
        //handle calls for specific pin
        if (value.empty()){
          bool isHigh = isPinHigh(expPinByParam);
          jsonRef["pinState"] = isHigh ? "on" : "off";
          jsonRef["pinNum"] = expPinByParam + 1;
          appendJsonStatus(jsonRef, true, "Read pin ok");
          return;
        } else {
          bool is_succ = tryWritePinByParam(expPinByParam, value);
          if (is_succ){
            appendJsonStatus(jsonRef, true, "Write pin ok");
            return;
          } else {
            appendJsonStatus(jsonRef, false, "Write pin failed");
            return;
          }
        }
      } else if (parameter == "bits"){
        if (value.length() == 0){
          //read pins
          appendJsonStatus(jsonRef, true, "Read bits ok");
          jsonRef["bits"] = get_output_bits();
          return;
        }
        int bits = intFromString(value);
        if ((bits < 0)||(bits > 0xFFFF)){
          appendJsonStatus(jsonRef, false, "ERR: invalid bits");
          return;
        } else {
          set_output_bits((uint16_t)bits);
          appendJsonStatus(jsonRef, true, "Write bits ok");
          return;
        }
      } else {
        appendJsonStatus(jsonRef, false, "ERR: invalid parameter");
        return;
      }
    }

    void getState(JsonObject& jsonRef){
      JsonObject currentTagData = jsonRef.createNestedObject(tag);
      currentTagData["type"] = "output";
      currentTagData["bits"] = get_output_bits();
      currentTagData["ioNum"] = out_num;
      return;
    }

    void resetOutputs() override{
      set_output_bits(0x00);
    }

  private:
    PCA9555* expander;
    int out_num;
    int out_offs;
};

class I_group: public IoGroup {
  public:
    I_group(
        antControllerIoType_t ioType,
        int pin_in_buff_ena,
        const std::vector<uint8_t> *pins )
    : IoGroup(ioType){
        this->pin_in_buff_ena = pin_in_buff_ena;
        this->pins = pins;
        enable();
    }

    retCode_t enable(){
        for (int iInput: *pins){
            pinMode(iInput, INPUT_PULLDOWN);
        }

        pinMode(pin_in_buff_ena,OUTPUT);
        digitalWrite(pin_in_buff_ena,0);
        return RET_OK;
    }

    bool isPinHigh(int pin_num){
        if(pin_num >= pins->size()){
            ALOGE("Pin {} is out of range", pin_num);
            return false;
        }
        return digitalRead((*pins)[pin_num]);
    }

    uint16_t get_input_bits(){
      uint16_t res = 0x00;
      for (int iInput = 0; iInput < pins->size(); iInput++){
        if (isPinHigh(iInput)){
          res |= (uint16_t)0x01<<iInput;
        }
      }
      return res;
    }

    void resetOutputs(){};

    void ioOperation(DynamicJsonDocument& jsonRef){
      std::string parameter = jsonRef["parameter"].as<std::string>();
      std::string value = jsonRef["value"].as<std::string>();
      if (parameter == "bits"){
        appendJsonStatus(jsonRef, true, "Read ok");
        jsonRef["bits"] = get_input_bits();
        return;
      } else {
        appendJsonStatus(jsonRef, false, "ERR: invalid parameter");
        return;
      }
    }

    void getState(JsonObject& jsonRef){
      JsonObject currentTagData = jsonRef.createNestedObject(tag);
      currentTagData["type"] = "input";
      currentTagData["bits"] = get_input_bits();
      currentTagData["ioNum"] = pins->size();
      return;
    }

private:
    int pin_in_buff_ena;
    const std::vector<uint8_t> *pins;
};


class ButtonHandler;

class IoController {

public:
    IoController() : buttonHandler(this) {};
    void begin(TwoWire &wire){
      _wire = &wire;
      init_controller_objects();
      spawnWatchdogTask();
    }
    DynamicJsonDocument handleApiCall(std::vector<std::string>& api_call);
    void setOutput(antControllerIoType_t ioType, int pin_num, bool val);
    bool getIoValue(antControllerIoType_t ioType, int pin_num);
    DynamicJsonDocument getIoControllerState();
    DynamicJsonDocument returnApiUnavailable(DynamicJsonDocument& jsonRef);

    void setLocked(bool shouldLock);
    void setPanic(bool shouldPanic);
    void attachNotifyTaskHandle(TaskHandle_t taskHandle);
    void notifyAttachedTask();
    void spawnWatchdogTask();

    bool locked;
    bool inPanic = false;
    TaskHandle_t notifyTaskHandle = NULL;

private:
    TwoWire* _wire;
    PCA9555 expanders[EXP_COUNT];

    std::vector<IoGroup*> ioGroups;
    ButtonHandler buttonHandler;

    retCode_t init_controller_objects();
    retCode_t init_expander(PCA9555* p_exp, int addr);

    void setDefaultState();
};

#endif // IO_CONTROLLER_H