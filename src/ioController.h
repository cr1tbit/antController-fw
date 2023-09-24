#ifndef IO_CONTROLLER_H
#define IO_CONTROLLER_H

#include <Arduino.h>
#undef B1
#include <Wire.h>

#include <vector>

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
    std::string apiAction(std::vector<std::string>& api_call){
      ALOGI("API call for tag {}",
        tag.c_str(), api_call.size());
      
      std::string parameter, value;

      switch (api_call.size()){
        case 0:
        case 1:
          return "ERR: no parameter";
        case 2:
          parameter = api_call[1];
          value = "";
          ALOGI("Parameter: {}, no value",
            parameter.c_str()
          );
          break;
        case 3:
          parameter = api_call[1];
          value = api_call[2];
          ALOGI("Parameter: {}, Value: {}",
            parameter.c_str(),
            value.c_str()
          );
          break;
        default:
          return "ERR: too many parameters";
      }
      
      return ioOperation(parameter, value);
    }

    std::string getState(){
      return ioOperation("bits", "");
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

    antControllerIoType_t ioType;
    std::string tag;
    virtual void resetOutputs() = 0;

  private:
    virtual std::string ioOperation(std::string parameter, std::string value) = 0;
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

      ALOGD("set pin {} {} @ {}",
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

    std::string ioOperation(std::string parameter, std::string value){
      bool is_succ = false;

      int parameter_int_offs = intFromString(parameter);
      if (parameter_int_offs > 0){
        //handle calls for specific pin
        parameter_int_offs -= 1; // we want pin numbering from 1
        if (value.length() == 0){
          return "OK: " + std::to_string(
            expander->read(
              (PCA95x5::Port::Port) parameter_int_offs));
        } else if (value.find("on") != std::string::npos) {
          is_succ = set_output(parameter_int_offs, true);
        } else if (value.find("off") != std::string::npos) {
          is_succ = set_output(parameter_int_offs, false);
        } else {
          return "ERR: invalid value";
        } 
      } else if (parameter == "bits"){
        if (value.length() == 0){
          return "OK: " + std::to_string(get_output_bits());
        }
        int bits = intFromString(value);
        if ((bits < 0)||(bits > 0xFFFF)){
          return "ERR: invalid bits value";
        }
        is_succ = set_output_bits((uint16_t)bits);
      } else {
        return "ERR: invalid parameter";
      }

      return is_succ ? "OK" : "ERR";
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
            attachInterrupt(iInput, input_pins_isr, CHANGE);
        }

        pinMode(pin_in_buff_ena,OUTPUT);
        digitalWrite(pin_in_buff_ena,0);
        return RET_OK;  
    }

    std::string ioOperation(std::string parameter, std::string value){
        if (parameter == "bits"){
            uint16_t bits;
            get_input_bits(&bits);
            return std::to_string(bits);
        } else {
            return "ERR: only bitwise read supported";
        }
    }

    bool get_input_bits(uint16_t* res){
        *res = 0;

        for (int iInput = 0; iInput < pins->size(); iInput++){
            uint8_t pin_to_read = (*pins)[iInput];
            // Serial.printf("read %d!\n\r",pin_to_read);
            if (digitalRead(pin_to_read) == true){
            // Serial.printf("%d ishigh !\n\r",pin_to_read);
            *res |= (uint16_t)0x01<<iInput;
            }
        }
        return true;
    }

    void resetOutputs(){};

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
    }
    std::string handleApiCall(std::vector<std::string>& api_call);
    void setOutput(antControllerIoType_t ioType, int pin_num, bool val);
    std::string getIoState(std::vector<std::string>& api_call);

private:
    TwoWire* _wire;
    PCA9555 expanders[EXP_COUNT];

    std::vector<IoGroup*> ioGroups;
    ButtonHandler buttonHandler;

    retCode_t init_controller_objects();    
    retCode_t init_expander(PCA9555* p_exp, int addr);  
};

#endif // IO_CONTROLLER_H