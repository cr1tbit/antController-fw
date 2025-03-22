#include "configHandler.h"
#include "gracefulRestart.h"

Config_ &Config = Config.getInstance();

const char CONFIG_FALLBACK[] = "/buttons_simple.conf";

static bool loadDefault(const char *name){
    Config.clearPresets();
    if (Config.loadConfig("/pins.conf") == false){
        return false;
    }
    if (Config.loadConfig(name) == false){
        return false;
    }
    Config.config_filename = name; //frontend only needs button config
    return true;
}

static bool loadFallback(){
    Config.clearPresets();
    if (Config.loadConfig(CONFIG_FALLBACK) == false){
        ALOGW("WARNING - Fallback config failed.");
        return false;
    } else {
        ALOGW("WARNING - Fallback config loaded.");
        Config.config_filename = CONFIG_FALLBACK;
        return true;
    }
}

// vTaskDelete may prevent resource freeing
static void configRAIIScope(const char* config){    
    if (loadDefault(config) == false){
        loadFallback();
        return;
    }
}

void configLoaderTask(void *parameter)
{
    //  = (const char*) parameter; 
    ALOGV("configLoaderTask start");
    configRAIIScope((const char*) parameter);

    ALOGI("TomlTask done. Connecting to WiFi...");
    vTaskDelete(NULL);
}