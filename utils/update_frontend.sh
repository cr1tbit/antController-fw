

# default paritioning scheme for 4MB

# Name,   Type, SubType, Offset,  Size, Flags
# nvs,      data, nvs,     0x9000,  0x5000,
# otadata,  data, ota,     0xe000,  0x2000,
# app0,     app,  ota_0,   0x10000, 0x140000,
# app1,     app,  ota_1,   0x150000,0x140000,
# spiffs,   data, spiffs,  0x290000,0x160000,
# coredump, data, coredump,0x3F0000,0x10000,

# The old way
# /home/critbit/.platformio/packages/tool-mkspiffs/mkspiffs_espressif32_arduino -c frontend -s 0x160000 /tmp/spiffs.bin
# esptool.py write_flash 0x290000 /tmp/spiffs.bin
#
# The new way

# 352 = 0x160 - exact number of 4096b blocks in 0x160000 flash area
littlefs_create -i /tmp/littlefs.bin -c 352 -s frontend
esptool.py write_flash 0x290000 /tmp/littlefs.bin
