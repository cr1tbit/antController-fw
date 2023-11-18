#pragma once

#include <vector>
#include <string>

#include <Arduino.h>
#include <FS.h>

#include "alfalog.h"

std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::string::size_type start = 0;
    std::string::size_type end = str.find(delimiter);

    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }

    result.push_back(str.substr(start));

    return result;
}

void listDir(fs::FS &fs, const char * dirname, uint8_t currentLvl = 0){
    const int maxLevel = 3;
    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            if(currentLvl < maxLevel){
                // ALOGR("{}:", file.path());
                listDir(fs, file.path(), currentLvl + 1);
            } else {
                ALOGT("LOL");
            }
        } else {
            ALOGD_RAW("{}: {}{}",
                file.path(),
                (file.size() > 2000)? file.size()/1024 : file.size(),
                (file.size() > 2000)? "kb" : "b");
        }
        file = root.openNextFile();
    }
}


// once we move to espidf this will work.
// void getStackUsed(){
//     TaskHandle_t xHandle;
//     TaskStatus_t xTaskDetails;

//     xHandle = xTaskGetHandle( "toml task" );
//     configASSERT( xHandle );

//     vTaskGetInfo(xHandle, &xTaskDetails, pdTRUE, eInvalid );

//     ALOGV("watermark is {}",xTaskDetails.usStackHighWaterMark);
// }