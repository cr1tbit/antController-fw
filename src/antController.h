#pragma once

#include <Arduino.h>

void SerialTerminalTask( void * parameter );
void TomlTask( void * parameter );
String handle_api_call(const String &subpath, int* ret_code);