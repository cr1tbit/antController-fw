#pragma once

const int PIN_I2C_SCL = 4;
const int PIN_I2C_SDA = 5;
const int PIN_I2C_RST = 12;

// board inputs

const uint8_t PIN_INPUT_1 = 14;
const uint8_t PIN_INPUT_2 = 15;
const uint8_t PIN_INPUT_3 = 16;
const uint8_t PIN_INPUT_4 = 17;

const uint8_t PIN_INPUT_5 = 18;
const uint8_t PIN_INPUT_6 = 19;
const uint8_t PIN_INPUT_7 = 21;
const uint8_t PIN_INPUT_8 = 22;

const uint8_t PIN_INPUT_9 = 23;
const uint8_t PIN_INPUT_10 = 25;
const uint8_t PIN_INPUT_11 = 26;
const uint8_t PIN_INPUT_12 = 27;

const uint8_t PIN_IN_BUFF_ENA = 13;

const std::vector<uint8_t> input_pins = {
  PIN_INPUT_1,
  PIN_INPUT_2,
  PIN_INPUT_3,
  PIN_INPUT_4,
  PIN_INPUT_5,
  PIN_INPUT_6,
  PIN_INPUT_7,
  PIN_INPUT_8,
  PIN_INPUT_9,
  PIN_INPUT_10,
  PIN_INPUT_11,
  PIN_INPUT_12
};

// user interface
const int PIN_BOOT_BUT1 = 0;
const int PIN_BUT2 = 35;
const int PIN_BUT3 = 34;
const int PIN_BUT4 = 33;

const int PIN_LED_STATUS = 2;