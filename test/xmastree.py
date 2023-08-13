import serial
from time import sleep
import sys
import random


# configure the serial connection
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=0.2)

# define the commands to send
commands = [
    "MOS",
    "MOS/1",
    "MOS/1/on",
    "MOS/1/off",
    "MOS/bits/",
    "REL/bits/44",
    "OPT/bits/0",
    "DUPA/bits/0",
    "INP/",
    "INP/bits",
    "INP/bits/1"
]

outputs = {
    "MOS": 16,
    "REL": 15,
    "OPT": 8,
    "TTL": 8
}

# clear the RX buffer
ser.reset_input_buffer()

if len(sys.argv) > 1:
    commands = sys.argv[1:]


# send each command and print the response
while True:
    for key, value in outputs.items():

        ser.write(
            ("{}/bits/{}\n".format(
                key,
                random.randint(0, 2**value - 1)
                )
            ).encode()
        )

        # print(ser.readline().decode().strip())
        # sleep(0.1)
# close the serial connection
ser.close()