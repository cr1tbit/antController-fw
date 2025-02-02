![Build status](https://github.com/cr1tbit/antcontroller-fw/actions/workflows/create-binary.yml/badge.svg)

# antController-fw

## Getting started

This project utilizes platformio IDE, which can be obtained by downloading platformio extension for visual studio code.

The project configuration sits in `platformio.ini`. In order for platformio to recognise it, open the directory that contains it. Then all the dependencies should be automatically installed, and the project should build effortlessly.

In order to get the board working, user must flash both firmware and filesystem partition.

This can be done via platformio GUI. For creating a release binary, a simple helper script has been included. User must run 3 commands from platformio terminal:

``` bash
# build filesystem image with files from data/ dir
pio run -e antcontroller -t buildfs
# merge firmware binary with the filesystem - merged-flash.bin will be created
pio run -e antcontroller -t mergebin
# flash merged-flash.bin to the ESP
pio run -e antcontroller -t flashall
```

User can also flash an binary artifact that can be found under `Actions` tab on repo.

``` bash
esptool.py write_flash 0x0 merged-flash.bin
```

## Logging

The logs can be seen either via Serial terminal, or on OLED display.

To see the output on serial, there are a couple of possibilities:

* Platformio builtin serial monitor (power socket icon at the bottom bar of VSCode)
* putty (windows)
* picocom (linux only) - `picocom /dev/ttyUSB0 -b115200`, exit via `ctrl + A + X` :)

## Internet connectivity

This project utilizes `WifiSettings` library, which will create an hotspot in case there's no valid WiFi configuration. After putting the credentials in, the board must be restarted via button on the webpage.

User can pre-program their own credentials, by creating 2 files in `data/` directory: 
 * `wifi-ssid` with network name
 * `wifi-password` with password.

Storing your network credentials in an unencrypted file, either on your desktop machine, or esp32's (external and unencrypted!) flash chip is a potential vector of attack, keep that in mind. As always, S in IoT stands for security.

After connecting to the network succesfully, to get the device's IP you can peek through logs on serial port, or check the top bar on the OLED screen (if available).

## API description

All the API can be accessed either via HTTP request, or via serial console.

The available endpoints are:

| Name| Ch. num | Description         |
| --- | ---     | ----------          |
| MOS | 16      | Mosfet bank         |
| REL | 15      | Relay bank          |
| OPT | 8       | Optocouplers        |
| TTL | 8       | 5V outputs          |
| INP | 16      | Inputs              |
| BUT | 4(a-d)  | Presets from config |

### Call via HTTP

On the device's IP there's a frontend website available, but user
can manually use the device's web API without it.

You can use either a browser or a shell tool like `curl`

For example, to set all optocouplers to "1", use:

`curl 192.168.0.145/api/OPT/bits/127`

or navigate to http://192.168.0.145/api/OPT/bits/127

    Important note - some browsers (like edge) hide the "http" part which forces the browser
    to use "https" - this means encrypted connection which ESP can't handle. In case of trouble,
    try to manually edit the https to http on browser bar, and/or use firefox.

### Call via Serial

Remember to omit the `/api/` part in this case, just type

`OPT/bits/127`

Which should result in something like:

```
D: Analyzing subpath: OPT/bits/127
I: API call for tag OPT
I: Parameter: bits, Value: 127
I: Write bits 0x7f on OPT
E: Op result: OK
```

## IO config

Antenna output configuration is stored in buttons.conf file. It has `.toml` syntax, but esp spiffs editor cannot view `.toml` files, so it has to be named `buttons.conf`

Buttons are split between groups, only one button per group can be active, group can also be set to `OFF`

### Loading configuration

The default config sits in /data directory. After editing or on first programming, the filesystem partition must be flashed, using `Project tasks -> Platform -> Upload Filesystem Image` that can be found in platformio extension sidebar

### Editing presets via WebServer

The config can be edited by navigating to `<IP_ADDR>/edit`, for now user/pass is `test`/`test`. The board always loads `button.conf` file on boot, if it's not present, the config will fail.

Be careful, this functionality is provided by `ESPAsyncWebServer` and tends to be buggy sometimes.

### Config file description

There are 2 kinds of objects there:

``` toml
[[pin]]
sch =     "Z2-7"    <- custom ID name, based from original schematic
antctrl = "SINK1"   <- name on antcontroller, must be: SINKx, RLx, OCx, TTLx or INPx
name =    "QROS"    <- function name of this pin
descr =   ""        <- optional description for this pin
```

``` toml
[[buttons.a]]              <- buttons.<group> 
name =  "160m VERTICAL"    <- name that will be called by `/BUT/<name>`
descr = ""                 <- optional description for this pin
pins = [ "Z2-7", "Z1-1" ]  <- pins activated by this button, must match <sch> or <name> of a pin.
disable_on_low = ["Z2-2"]  <- a button might be dangerous to use in some situations. These 2 settings
disable_on_high = ["Z2-3"] <- allow the user to disable / turn off automatically a button when a pin 
                              is / becomes in this specified state.  (CAUTION: does not work with inputs pins for now)
```

The config is parsed at the start of the board, if any settings are invalid, the buttons will not function.

## Button API

To activate a button preset

`/api/BUT/<group[a-d]>/<button_name>`

or to reset a group

`/api/BUT/<group[a-d]>/OFF`

## Invalid preset protection features

Antcontroller provides a method to prevent undesirable system configurations. For example, some presets may damage an amplifier, if 
currently a wrong antenna is connected.

The first method is provided by the button groups itself - only one button in a group may be active, and they work in a "break-before-make".
Please note - there are no guarantees, how big is the time gap between disabling prevous and the current button, but it should not exceed 1ms in normal circumstances.

The second method is set up by adding a "disable_on_low" or "disable_on_high" parameter for the button. This way, user may specify when a specific preset must not be active, when a pin is in a specific state. This is called a "pinGuard", and requires a bit more explanation.

### Pin guards

At the start of the program, when parsing each button config, we gather every pin from `disable_on_low/high`. Then, a "guard" is attached to it.

After reading the configuration is complete, each pin may "guard" one or more buttons. A single "pinGuard" looks like this:

``` C
typedef struct {
    bool onHigh;
    std::string guardedButton;
} pinGuard_t;
```

The pinGuard my be triggered in 3 ways:

1. The system will prevent activating a button, if it's prevented by any pinGuard.
2. When a button is pressed, each pin that will be activated will run a pinGuard check, and disable any other conflicting buttons, before finally activatng the requested one (thus handling the break-before-make principle) - TODO: NOT TRUE CURRENTLY
3. When any input pin changes its' state, pinGuards for each input pins will be reevaluated, and conflicting buttons will be immediatelly turned off.


Words of caution:
1. currently "disable_on_low" guard will not work properly for the output pins. Fixing this requires a complicated architecture rewrite in the future.
2. Inputs state is being updated every 25 miliseconds, this introduces a delay between an external signal happening, and a guarded button being disabled. This also means, that signals shorter than 25ms might not be detected. The operating frequency may be changed in the code, but setting it too low may starve the resources too much.
3. PinGuards for the input pins are only asserted, when any of the inputs change. However, currently checking the input pinGuards is not very optimal (and needs another rewrite). When any input pin (not nessesarily with a pinGuard attached) is floating, or connected to a fast-changing signal, it may seriously slow down or even crash the device.

This rewrite is needed, because:
1. Interfacing between groups/pins/guards is based on "string" names, not the binary structures. This means a lot of "strcmp" operations
2. Mentioned above objects have been defined in a very chaotic way. TODO: explain better
3. Some operations are not optimized, and require traversing object vectors in 3-level "for" loops.
4. Software debounce should be added to input watchdog task.

### Special inputs

IO15 (INP2) - enables "lock" mode - this disables pressing any button while this pin is high
IO16 (INP3) - enables "panic" mode (on low) - after activating, each preset will go to the default state (all buttons off)