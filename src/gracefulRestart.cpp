
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