# antController-fw


This device uses state-of-the-art logging library `Alfalog` (Which I'm developing in parallel to this project)

The logs can be seen either via Serial terminal, or on OLED display.

To see the output on serial, there are a couple of possibilities:

* Platformio builtin serial monitor (power socket icon at the bottom bar of VSCode)
* putty (windows)
* picocom (linux only) - `picocom /dev/ttyUSB0 -b115200`, exit via `ctrl + A + X` :)


## Web server

To get the device's IP you can peek through logs on serial port, or check the top LCD bar (if available).


## API description

All the API can be accessed either via HTTP request, or via serial console.

The available endpoints are:

| Name| Ch. num | Description   |
| --- | ---     | ----------    |
| MOS | 16      | Mosfet bank   |
| REL | 15      | Relay bank    |
| OPT | 8       | Optocouplers  |
| TTL | 8       | 5V outputs    |
| INP | 16      | Inputs        |


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

