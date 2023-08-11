#pragma once 

#include <Arduino.h>
#include <Wire.h>
#include <PCA95x5.h>

#include <vector>

const int EXP_MOS_ADDR = 0x20;
const int EXP_REL_ADDR = 0x21;
const int EXP_OPTO_ADDR = 0x22;


typedef enum {
  RET_OK = 0,
  RET_ERR = -1
} ret_code_t;

typedef enum {
  MOSFET = 0,
  RELAY,
  OPTO,
  TTL,
  OUT_TYPE_COUNT
} output_group_type_t;

typedef enum {
  EXP_MOSFETS = 0,
  EXP_RELAYS,
  EXP_OPTO_TTL,
  EXP_COUNT
} exp_index_t;

typedef struct {
  PCA9555* p_exp;
  int out_num;
  int out_offs;
} output_group_t;

bool pins_changed = false;

void IRAM_ATTR input_pins_isr() {
  pins_changed = true;
  // digitalWrite(PIN_LED_STATUS,~digitalRead(PIN_LED_STATUS));
}

class IoGroup {

  protected:
    IoGroup(String tag){
      this->tag = tag;
    }

  public:
    String api_action(std::vector<String>& api_call){
      Serial.printf("API call for tag %s, path len %d\n\r",tag.c_str(),api_call.size());
      
      String parameter, value;

      if (api_call.size() == 1){
        return "ERR: no parameter";
      } else if (api_call.size() == 2){
        parameter = api_call[1];
        value = "";
      } else if (api_call.size() == 3){
        parameter = api_call[1];
        value = api_call[2];
      }

      Serial.printf("Parameter %s, Value %s\n\r",parameter.c_str(),value.c_str());
      return ioOperation(parameter, value);
    }

    int intFromString(String& str){
      if (str.length() == 0){
        return -1;
      }
      //check if string contains any non-digit characters
      for (int i = 0; i < str.length(); i++){
        if (!isDigit(str.charAt(i))){
          return -1;
        }
      }
      return str.toInt();
    }
    
    String tag;

  private:
    virtual String ioOperation(String parameter, String value) = 0;
};


class O_group : public IoGroup {
  public:
    O_group(String tag, PCA9555* p_exp, int out_num, int out_offs): IoGroup(tag){
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

      Serial.printf(
        "set pin %d %s @ %s\n\r",
        offs_pin,
        val ? "on" : "off",
        tag.c_str());

      expander->write(
        (PCA95x5::Port::Port) offs_pin,
        (PCA95x5::Level::Level )val
      );
      return true;
    }

    bool set_output_bits(uint16_t bits){
      int bit_range = pow(2,out_num);
      if (bits > bit_range){
        // bits are out of range
        Serial.printf("Cannot write bits %04x as it exceeds 0x%04x on %s\n\r",
          bits, bit_range, tag.c_str());
        return false;
      }   
      bits &= (0xFFFF >> 16-out_num);      
      bits <<= out_offs;
      bits |= expander->read() & ~(0xFFFF >> 16-out_num << out_offs);
      Serial.printf("Write bits %04x on %s\n\r",bits, tag.c_str());

      expander->write(bits);
      return true;
    }

    String ioOperation(String parameter, String value){
      bool is_succ = false;

      int parameter_int_offs = intFromString(parameter);
      if (parameter_int_offs > 0){
        //handle calls for specific pin
        parameter_int_offs -= 1; // we want pin numbering from 1
        if (value.length() == 0){
          return "OK: " + String(expander->read((PCA95x5::Port::Port)parameter_int_offs));
        } else if (value.indexOf("on") >= 0) {
          is_succ = set_output(parameter_int_offs, true);
        } else if (value.indexOf("off") >= 0) {
          is_succ = set_output(parameter_int_offs, false);
        } else {
          return "ERR: invalid value";
        } 
      } else if (parameter == "bits"){
        if (value.length() == 0){
          return "OK: " + String(expander->read());
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

  private:
    PCA9555* expander;
    int out_num;
    int out_offs;  
};

class I_group: public IoGroup {
  public:
    I_group(String tag, int pin_in_buff_ena, const std::vector<uint8_t> *pins): IoGroup(tag){
      this->pin_in_buff_ena = pin_in_buff_ena;
      this->pins = pins;
      enable();
    }

    ret_code_t enable(){
      for (int iInput: *pins){  
        pinMode(iInput, INPUT_PULLDOWN);
        attachInterrupt(iInput, input_pins_isr, CHANGE);
      }

      pinMode(pin_in_buff_ena,OUTPUT);
      digitalWrite(pin_in_buff_ena,0);
      return RET_OK;  
    }

    String ioOperation(String parameter, String value){
      if (parameter == "bits"){
        uint16_t bits;
        get_input_bits(&bits);
        return String(bits);
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

  private:
    int pin_in_buff_ena;
    const std::vector<uint8_t> *pins;
};

class IoController_ {

  private:
    IoController_() = default;

    TwoWire* _wire;
    PCA9555 expanders[EXP_COUNT];

    std::vector<IoGroup*> groups;

    ret_code_t init_controller_objects(){
      init_expander(&expanders[EXP_MOSFETS], EXP_MOS_ADDR );
      init_expander(&expanders[EXP_RELAYS],  EXP_REL_ADDR );
      init_expander(&expanders[EXP_OPTO_TTL],EXP_OPTO_ADDR);

      groups.push_back(new O_group("MOS", &expanders[EXP_MOSFETS], 16, 0));
      groups.push_back(new O_group("REL", &expanders[EXP_RELAYS],  15, 0));
      groups.push_back(new O_group("OPT", &expanders[EXP_OPTO_TTL], 8, 8));
      groups.push_back(new O_group("TTL", &expanders[EXP_OPTO_TTL], 8, 0));
      groups.push_back(new I_group("INP", PIN_IN_BUFF_ENA, &input_pins));

      return RET_OK;
    }

    ret_code_t init_expander(PCA9555* p_exp, int addr){
      p_exp->attach(*_wire,addr);

      p_exp->polarity(PCA95x5::Polarity::ORIGINAL_ALL);
      p_exp->direction(PCA95x5::Direction::OUT_ALL);
      bool write_status = p_exp->write(PCA95x5::Level::L_ALL);

      Serial.printf("init exp %02x ",addr);

      if (write_status){
        Serial.println("Expander OK!");
        return RET_OK;
      }
      Serial.println("Expander ERR!");
      return RET_ERR;
    }

  public:

    // singleton pattern as from 
    // https://forum.arduino.cc/t/how-to-write-an-arduino-library-with-a-singleton-object/666625
    static IoController_ &getInstance(){
        static IoController_ instance;
        return instance;
    }

    IoController_(const IoController_ &) = delete; // no copying
    IoController_ &operator=(const IoController_ &) = delete;

    void begin(TwoWire &wire){
      _wire = &wire;
      init_controller_objects();
    }

    String handleApiCall(std::vector<String>& api_call){
      
      for(IoGroup* group: groups){
        if (group->tag == api_call[0]){
          return group->api_action(api_call);
        }
      }
      String result = "ERR: API call for tag " + api_call[0] + " not found";
      Serial.println(result);
      return result;
    }
};

IoController_ &IoController = IoController.getInstance();