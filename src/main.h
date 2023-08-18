#pragma once

#include <Arduino.h>
#undef B1

void SerialTerminalTask( void * parameter );
void TomlTask( void * parameter );
String handle_api_call(const String &subpath, int* ret_code);