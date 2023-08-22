Import("env")

def getMergeBinCommand():
    command = "\
    esptool.py --chip ESP32 merge_bin\
        -o merged-flash.bin --flash_size=keep"
    
    for addr,path in env["FLASH_EXTRA_IMAGES"]:
        command += " {} {} ".format(addr, path)

    command += env["ESP32_APP_OFFSET"] + " $BUILD_DIR/firmware.bin"    
    command += " 0x310000 $BUILD_DIR/littlefs.bin"
    return command    

env.AddCustomTarget(
    name="mergebin",
    dependencies=[
        "$BUILD_DIR/partitions.bin",
        "$BUILD_DIR/bootloader.bin",
        "$BUILD_DIR/firmware.bin"
    ],
    actions=[
        getMergeBinCommand()
    ],
    title="Get merged binary",
    description=""
)

env.AddCustomTarget(
    name="flashall",
    dependencies=[
        "merged-flash.bin"
    ],
    actions=[
        "esptool.py write_flash 0x0 merged-flash.bin"
    ],
    title="Flash merged binary",
    description=""
)


# 0x00000 $BUILD_DIR/bootloader.bin\
# 0x8000 $BUILD_DIR/partitions.bin\
# 0xe000 /home/critbit/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin\
# 0x10000 $BUILD_DIR/firmware.bin