
#include "ioController.h"
#include "ioControllerTypes.h"

void IRAM_ATTR input_pins_isr() {
  static bool pins_changed = false;
  pins_changed = true;
  // digitalWrite(PIN_LED_STATUS,~digitalRead(PIN_LED_STATUS));
}

bool isOutputType(antControllerIoType_t ioType){
  return ioType == MOSFET ||
         ioType == RELAY ||
         ioType == OPTO ||
         ioType == TTL;
}


retCode_t IoController::init_controller_objects(){
    init_expander(&expanders[EXP_MOSFETS], EXP_MOS_ADDR );
    init_expander(&expanders[EXP_RELAYS],  EXP_REL_ADDR );
    init_expander(&expanders[EXP_OPTO_TTL],EXP_OPTO_ADDR);

    ioGroups.push_back(new O_group(MOSFET,&expanders[EXP_MOSFETS], 16, 0));
    ioGroups.push_back(new O_group(RELAY, &expanders[EXP_RELAYS],  15, 0));
    ioGroups.push_back(new O_group(OPTO,  &expanders[EXP_OPTO_TTL], 8, 8));
    ioGroups.push_back(new O_group(TTL,   &expanders[EXP_OPTO_TTL], 8, 0));
    ioGroups.push_back(new I_group(INP,   PIN_IN_BUFF_ENA, &input_pins));

    for (auto &e: expanders){
        e.write(PCA95x5::Level::L_ALL);
    }
    for (auto bGroup: Config.button_groups){
        buttonHandler.resetOutputsForButtonGroup(bGroup.first);
    }

    return RET_OK;
}

retCode_t IoController::init_expander(PCA9555* p_exp, int addr){
    p_exp->attach(*_wire,addr);

    p_exp->polarity(PCA95x5::Polarity::ORIGINAL_ALL);
    p_exp->direction(PCA95x5::Direction::OUT_ALL);
    bool write_status = p_exp->write(PCA95x5::Level::L_ALL);

    ALOGT("init exp {:#02x}", addr);

    if (write_status){
        ALOGT("Expander OK!");
        return RET_OK;
    }
    ALOGE("Expander {:#02x} Error! Check the connections", addr);
    return RET_ERR;
}


DynamicJsonDocument IoController::getIoControllerState(){
    DynamicJsonDocument retJson(1024);
    JsonObject ioArray = retJson.createNestedObject("io");

    for (auto& g: ioGroups){
        g->getState(ioArray);
    }
    buttonHandler.getState(retJson);

    retJson["msg"] = "OK";
    retJson["retCode"] = 200;
    ALOGI("Json bufer {}/{}b", retJson.memoryUsage(), retJson.capacity());

    return retJson;
}

DynamicJsonDocument IoController::handleApiCall(std::vector<std::string>& api_call){
    DynamicJsonDocument retJson(1024);

    if (api_call[0] == "BUT"){
        std::string ret = buttonHandler.apiAction(api_call)? "OK" : "ERR";
        retJson["msg"] = ret;
        return retJson;
    }
    if (api_call[0] == "INF"){
        return getIoControllerState();
    }
    for(IoGroup* group: ioGroups){
        if (group->tag == api_call[0]){
            return group->apiAction(api_call);
        }
    }
    retJson["msg"] = "ERR: API call for tag " + api_call[0] + " not found";
    return retJson;
}

void IoController::setOutput(antControllerIoType_t ioType, int pin_num, bool val){
    // ALOGD("enum {}", ioType);
    if (!isOutputType(ioType)){
        ALOGE("{} is not an output!", ioTypeMap.at(ioType));
        return;
    }
    for (auto& g: ioGroups){
        if (g->ioType == ioType){
            O_group* group = (O_group*)g;
            group->set_output(pin_num, val);
            return;
        }
    }
}