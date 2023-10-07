Import("env")

def getMergeBinCommand(outName: str):
    command = "\
    esptool.py --chip ESP32 merge_bin\
        -o " + outName + " --flash_size=keep"
    
    for addr,path in env["FLASH_EXTRA_IMAGES"]:
        command += " {} {} ".format(addr, path)

    command += env["ESP32_APP_OFFSET"] + " $BUILD_DIR/firmware.bin"    
    command += " 0x310000 $BUILD_DIR/littlefs.bin"
    return command    

env.AddCustomTarget(
    name="fwimage",
    dependencies=[
        "$BUILD_DIR/firmware.bin"
        "$BUILD_DIR/partitions.bin",
        "$BUILD_DIR/bootloader.bin"
    ],
    actions=[
        "cp -f $BUILD_DIR/firmware.bin firmware.bin"
        "cp -f $BUILD_DIR/partitions.bin partitions.bin"
        "cp -f $BUILD_DIR/bootloader.bin bootloader.bin"
    ],
    title="Get binaries separatelly (helper for CI)",
    description=""
)

env.AddCustomTarget(
    name="fullimage",
    dependencies=[
        "$BUILD_DIR/partitions.bin",
        "$BUILD_DIR/littlefs.bin",
        "$BUILD_DIR/bootloader.bin",
        "$BUILD_DIR/firmware.bin"
    ],
    actions=[
        getMergeBinCommand("fullimage.bin")
    ],
    title="Get binary with filesystem",
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