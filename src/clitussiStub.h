#pragma once

#include <vector>
#include <string>
#include <Arduino.h>

typedef std::function<void(std::string)> ClitussiCallback;

class clitussiStub {

private:
    const int clitussyDepth = 40;
    std::vector<char> buf;

    std::map<std::string, ClitussiCallback> Callbacks;

public:
    bool echo = false;

    clitussiStub(){
        this->attachCommandCb("help", [this](std::string cmd){
            Serial.println("Command Line Interface Under Shared Serial Interface stub");

            Serial.println("Registered commands:");
            for (auto& [filter, cb]: Callbacks){
                Serial.println(filter.c_str());
            }
        });
    }

    void attachCommandCb(std::string filter, ClitussiCallback cb){
        Callbacks[filter] = cb;
    }

    void handleCommand(const std::string& cmd){
        ALOGT("CLITUSSI HANDLE");
        for (auto& [filter, cb]: Callbacks){
            if ((filter.length() > 0) && (cmd.find(filter) == 0)){
                cb(cmd);
                return;
            } else {
                ALOGT("No match for filter \"{}\"", filter.c_str());
            }
        }
        //at last, try to call cb with empty string as key
        if (Callbacks.find("") != Callbacks.end()){
            ALOGT("call catchall handler");
            Callbacks[""](cmd);
        }
    }

    void putc(char c){
        if (echo) Serial.print(c);
    }

    void puts(const char* s){
        if (echo) Serial.print(s);
    }

    void loop(){
        int ret_code = 0;
        int charsInCurrentLoop = 0;

        while (Serial.available())
        {
            char c = Serial.read();
            charsInCurrentLoop++;
            switch (c) {
                case '\r':
                    this->putc('\r');
                    break;
                case '\b':
                    if (buf.size() > 0)
                    {
                        buf.pop_back();
                        this->puts("\b \b");
                    }
                    break;
                case '\n':
                    {
                    const std::string cmd(buf.begin(), buf.end());
                    handleCommand(cmd);
                    buf.clear();
                    break;
                    }
                default:
                {
                    this->putc(c);
                    if (buf.size() <= clitussyDepth)
                    {
                        buf.push_back(c);                        
                    } else {
                        this->puts("\b \b");
                    }
                }
            }
        }
        if ((echo == false) && (charsInCurrentLoop == 1)){
            echo = true;
            puts("\nSlow interaction detected, enabling echo. Type \"help\" for more. \n");
            //redraw buff
            for (auto c: buf){
                putc(c);
            }
        }
    }
};