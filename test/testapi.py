import serial
from time import sleep
import sys
import json

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
    "INP/bits/1",
    "INF"
]

def handleResponse(response):
    
    try:
        jsonOnly = response[response.find("{"):response.rfind("}")+1]
        # print(jsonOnly)
        # try to parse the response as a JSON object
        jsonResp = json.loads(jsonOnly)
        #pretty-print it
        print (json.dumps(jsonResp, indent=4))
    except ValueError:
        print ("cannot parse response as JSON object:")
        print (response)


# clear the RX buffer
ser.reset_input_buffer()

if len(sys.argv) > 1:
    commands = sys.argv[1:]


# send each command and print the response
for command in commands:
    ser.write((command + '\n').encode())
    response = ''

    while True:
        data = ser.readline().decode().strip()
        if not data:
            break
        response += data + '\n'
    print("Command: " + command)
    handleResponse(response)

# close the serial connection
ser.close()