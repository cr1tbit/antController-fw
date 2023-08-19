#pragma once

#include <Arduino.h>
#undef B1

void SerialTerminalTask( void * parameter );
void TomlTask( void * parameter );
std::string handleApiCall(const std::string &subpath, int* ret_code);