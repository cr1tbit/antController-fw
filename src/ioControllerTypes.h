#ifndef IO_CONTROLLER_TYPES_H
#define IO_CONTROLLER_TYPES_H

#include <string>
#include <vector>
#include <map>

#include <fmt/core.h>
#include <PCA95x5.h>

typedef enum {
    RET_OK = 0,
    RET_ERR = -1
} retCode_t;

typedef enum {
    EXP_MOSFETS = 0,
    EXP_RELAYS,
    EXP_OPTO_TTL,
    EXP_COUNT
} expIndex_t;

typedef struct {
    PCA9555* p_exp;
    int out_num;
    int out_offs;
} outputGroup_t;


typedef enum {
    MOSFET = 0,
    RELAY,
    OPTO,
    TTL,
    INP,
    OUT_TYPE_COUNT
} antControllerIoType_t;

bool isOutputType(antControllerIoType_t ioType);


const std::map<antControllerIoType_t, const char*> ioTypeMap = {
    {MOSFET, "MOS"},
    {RELAY, "REL"},
    {OPTO, "OPT"},
    {TTL, "TTL"},
    {INP, "INP"}
};


typedef struct {
    bool onHigh;
    std::string guardedButton;
} pinGuard_t;

class pin_t{
public:
    std::string name;
    std::string sch;

    std::vector<pinGuard_t> guardedButtonNames;

    antControllerIoType_t ioType;
    int ioNum;

    bool operator==(const pin_t& other) const {
        return name == other.name;
    }

    std::string to_string() const{
        std::string ret = fmt::format("{}: {}[{}] ({})",
            name, ioTypeMap.at(ioType), ioNum, sch);
        
        if (guardedButtonNames.size() != 0){
            ret += " - guards buttons: ";
            for (auto& g : guardedButtonNames){
                ret += fmt::format("('{}'@{})", g.guardedButton,g.onHigh ? "high" : "low" );
            }
        }        
        return ret;
    }

    void setGuard(std::string buttonName, bool onHigh){
        pinGuard_t guard;
        guard.onHigh = onHigh;
        guard.guardedButton = buttonName;
        guardedButtonNames.push_back(guard);
    }

    int numFromName(const std::string& numText){
        std::string numStr;

        for (auto c : numText){
            if (isdigit(c)){
                numStr += c;
            }
        }

        if (numStr.length() == 0){
            return -1;
        } else {
            return std::stoi(numStr);
        }
    }

    pin_t() = delete;

    pin_t (
        const std::string& name,
        const std::string& antctrl,
        const std::string& sch
    ){
        this->name = name;
        this->sch = sch;

        if(antctrl.find("SINK") != std::string::npos){
            ioType = MOSFET;
        } else if(antctrl.find("RL") != std::string::npos){
            ioType = RELAY;
        } else if(antctrl.find("OC") != std::string::npos){
            ioType = OPTO;
        } else if(antctrl.find("TTL") != std::string::npos){
            ioType = TTL;
        } else if(antctrl.find("INP") != std::string::npos){
            ioType = INP;
        } else {
            const std::string err = fmt::format(
                "could not parse pin type from: {}", antctrl);
            throw std::runtime_error(err);
        }

        ioNum = numFromName(antctrl);
        if (ioNum < 0){
            const std::string err = fmt::format(
                "could not parse pin number from: {}", antctrl);
            throw std::runtime_error(err);
        }

        ioNum -= 1; //translate schematic numbers to code numbers
    }    
};

class button_t {
public:
    std::string name;
    std::vector<std::string> pinNames;

    button_t() = delete;

    button_t(
        const std::string& name,
        const std::vector<std::string>& pinNames
    ){
        this->name = name;
        this->pinNames = pinNames;
    }

    std::string to_string(){
        std::string ret = name + ": [";
        for(const auto& p : pinNames){
            ret += p + " ";
        }
        ret += "]";
        return ret;
    }
};

typedef struct {
    std::string name;
    std::vector<button_t> buttons;

    std::string to_string(){
        std::string ret = name + ": \n";
        for(auto& b : buttons){
            ret += b.to_string() + "\n";
        }
        return ret;
    }
    std::string currentButtonName = "OFF";
} buttonGroup_t;

#endif // IO_CONTROLLER_TYPES_H