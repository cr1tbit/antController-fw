
#include <Arduino.h>
#include "alfalog.h"


// freeRTOS task to restart the ESP32
static void restartTask(void *pvParameters) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
    while(1);
}

void gracefulRestart() {
    ALOGV("Restarting in 1s...");
    xTaskCreate(
        restartTask, "restartTask",
        1000, NULL, 1, NULL //TODO smaller
    );
}

const char* getResetReasonStr(){
    esp_reset_reason_t reset_reason = esp_reset_reason();
    switch (reset_reason){
        case ESP_RST_POWERON:
            return "Power on";
        case ESP_RST_EXT:
            return "External";
        case ESP_RST_SW:
            return "Software";
        case ESP_RST_PANIC:
            return "Exception";
        case ESP_RST_INT_WDT:
            return "Isr WDT";
        case ESP_RST_TASK_WDT:
            return "Task WDT";
        case ESP_RST_WDT:
            return "Other WDT";
        case ESP_RST_DEEPSLEEP:
            return "Deep sleep";
        case ESP_RST_BROWNOUT:
            return "Brownout";
        case ESP_RST_SDIO:
            return "SDIO";
        default:
            return "Unknown";
    }
}

bool lastRestartFaulty(){
    esp_reset_reason_t reset_reason = esp_reset_reason();
    switch (reset_reason){
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_BROWNOUT:
            return true;
        default:
            return false;
    }
}