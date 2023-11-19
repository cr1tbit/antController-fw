Import("env")
import os

frontend_addr = env.GetProjectOption("frontend_addr")

try:
    token = os.environ["GITHUB_TOKEN"]
except:
    token = ""

def getMergeBinCommand(outName: str):
    command = "\
    esptool.py --chip ESP32 merge_bin\
        -o " + outName + " --flash_size=keep"
    
    # for addr,path in env["FLASH_EXTRA_IMAGES"]:
    #     command += " {} {} ".format(addr, path)

    command += " 0x1000 bin/bootloader.bin"
    command += " 0x8000 bin/partitions.bin"    
    # command += env["ESP32_APP_OFFSET"] + " bin/firmware.bin"    
    command += " 0x10000 bin/firmware.bin"    
    command += " 0x310000 bin/littlefs.bin"
    return command    

env.AddCustomTarget(
    name="fetchfrontend",
    dependencies=[],
    actions=[
        "curl -L "        
            "-H \"Accept: application/vnd.github+json\" "
            "-H \"Authorization: Bearer " + token + "\" "
            "-H \"X-GitHub-Api-Version: 2022-11-28\" " + frontend_addr +
            " --output /tmp/front.zip",
        "unzip -ou /tmp/front.zip -d /tmp/front",
        "tar -xzf /tmp/front/build.tar.gz -C /tmp/front",
        "rm -rf data/static/*",
        "mkdir -p data/static",
        "mv -f /tmp/front/build/* data/static/.",
    ],
)

env.AddCustomTarget(
    name="fwimage",
    dependencies=[
        "$BUILD_DIR/firmware.bin",
        "$BUILD_DIR/partitions.bin",
        "$BUILD_DIR/bootloader.bin",
        "$BUILD_DIR/littlefs.bin"
    ],
    actions=[
        "mkdir -p bin",
        "cp -f $BUILD_DIR/firmware.bin bin/firmware.bin",
        "cp -f $BUILD_DIR/partitions.bin bin/partitions.bin",
        "cp -f $BUILD_DIR/bootloader.bin bin/bootloader.bin",
        "cp -f $BUILD_DIR/littlefs.bin bin/littlefs.bin"
    ],
    title="Get binaries separatelly (helper for CI)",
    description=""
)

env.AddCustomTarget(
    name="fullimage",
    dependencies=[
        "bin/partitions.bin",
        "bin/littlefs.bin",
        "bin/bootloader.bin",
        "bin/firmware.bin"
    ],
    actions=[
        getMergeBinCommand("bin/fullimage.bin")
    ],
    title="Get binary with filesystem",
    description=""
)

env.AddCustomTarget(
    name="flashall",
    dependencies=[
        "bin/merged-flash.bin"
    ],
    actions=[
        "esptool.py write_flash 0x0 bin/merged-flash.bin"
    ],
    title="Flash merged binary",
    description=""
)


# 0x00000 $BUILD_DIR/bootloader.bin\
# 0x8000 $BUILD_DIR/partitions.bin\
# 0xe000 /home/critbit/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin\
# 0x10000 $BUILD_DIR/firmware.bin