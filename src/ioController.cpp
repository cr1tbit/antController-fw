
#include "ioController.h"
#include "ioControllerTypes.h"
#include "gracefulRestart.h"
#include "commonFwUtils.h"

bool isOutputType(antControllerIoType_t ioType){
  return ioType == MOSFET ||
         ioType == RELAY ||
         ioType == OPTO ||
         ioType == TTL;
}

void IoController::setDefaultState(){
    for (auto &e: expanders){
        e.write(PCA95x5::Level::L_ALL);
    }
    for (auto bGroup: Config.button_groups){
        buttonHandler.resetOutputsForButtonGroup(bGroup.first);
    }
    locked = false;
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

    setDefaultState();
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

    retJson["locked"] = locked;
    retJson["panic"] = inPanic;
    retJson["msg"] = "OK";
    retJson["retCode"] = 200;
    ALOGT("Json bufer {}/{}b", retJson.memoryUsage(), retJson.capacity());

    return retJson;
}

DynamicJsonDocument IoController::returnApiUnavailable(DynamicJsonDocument& retJson){
    std::string msg = "Controller is ";

    if (inPanic){
        msg +="in panic mode";
    } else {
        msg += "locked";
    }
    ALOGW(msg.c_str());
    retJson["msg"] = msg;
    return retJson;

    retJson["msg"] = "ERR: " + msg;
    return retJson;
}   

DynamicJsonDocument IoController::handleApiCall(std::vector<std::string>& api_call){
    DynamicJsonDocument retJson(1024);

    if (api_call[0] == "INF"){
        return getIoControllerState();
    }
    if (api_call[0] == "RST"){
        setDefaultState();
        gracefulRestart();
        retJson["msg"] = "OK";
    }

    if (api_call[0] == "BUT"){
        if ((locked)||(inPanic)){ return returnApiUnavailable(retJson);}

        std::string ret = buttonHandler.apiAction(api_call)? "OK" : "ERR";
        retJson["msg"] = ret;
        return retJson;
    }
    for(IoGroup* group: ioGroups){
        if (group->tag == api_call[0]){
            if ((locked)||(inPanic)){ return returnApiUnavailable(retJson);}

            return group->apiAction(api_call);
        }
    }
    retJson["msg"] = "ERR: API call for tag " + api_call[0] + " not found";
    return retJson;
}

void IoController::setOutput(antControllerIoType_t ioType, int pin_num, bool val){
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

bool IoController::getIoValue(antControllerIoType_t ioType, int pin_num){
    for (auto& g: ioGroups){
        if (g->ioType == ioType){
            return g->isPinHigh(pin_num);
        }
    }
    ALOGE("IO type {} not found", ioTypeMap.at(ioType));
    return false;
}

uint16_t IoController::getGroupBits(antControllerIoType_t ioType){
    for (auto& g: ioGroups){
        if (g->ioType == ioType){
            return g->get_bits();
        }
    }
    return 0; //TODO handle errors?
}

void IoController::attachNotifyTaskHandle(TaskHandle_t taskHandle){
    notifyTaskHandle = taskHandle;
}

void IoController::notifyAttachedTask(){
    if (notifyTaskHandle != NULL){
        xTaskNotifyGive(notifyTaskHandle);
        ALOGT("Notifying task {}", notifyTaskHandle);
    }
}

void IoController::setPanic(bool shouldPanic){
    if (shouldPanic == inPanic) return;

    if (shouldPanic == false){
        ALOGV("exit panic mode.");
        inPanic = false;
    } else {
        setDefaultState();
        ALOGV("Panic! outputs set to default");
        inPanic = true;
    }

    notifyAttachedTask();
}

void IoController::setLocked(bool shouldLock){
    if (shouldLock == locked) return;
    locked = shouldLock;

    notifyAttachedTask();
}

void IoController::notifyOnBitsChange(uint16_t bits){
    static uint16_t lastBits = 0;

    if (bits == lastBits) return;
    lastBits = bits;

    //TODO: optimize this so only changed bits are checked.
    buttonHandler.recheckPinGuards(true);
    notifyAttachedTask();
}

void WatchdogTask(void *p_ioController){
    IoController* ioController = (IoController*)p_ioController;
    int loop = 0;

    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    for( ;; ){
        if (digitalRead(PIN_INPUT_1) == LOW){
            ioController->setLocked(true);
        } else {
            ioController->setLocked(false);
        }

        if (digitalRead(PIN_INPUT_3) == HIGH){
            ioController->setPanic(true);
        } else {
            ioController->setPanic(false);
        }
        ioController->notifyOnBitsChange(ioController->getGroupBits(INP));

        if (loop++ % 4 == 0){
            if (ioController->locked){
                handle_io_pattern(PIN_LED_STATUS, PATTERN_ERR);
            } else {
                handle_io_pattern(PIN_LED_STATUS, PATTERN_HBEAT);
            }    
        }        
        vTaskDelayUntil( &xLastWakeTime, 25 / portTICK_PERIOD_MS);
    }
}

void IoController::spawnWatchdogTask(){
    xTaskCreate( WatchdogTask, "IoC Watchdog",
            4000, this, 20, NULL );
}