

IP=192.168.0.145


#define array of parameters for each test

callArray=(
    ""
    "MOS"
    "MOS/1"
    "MOS/1/on"
    "MOS/1/off"
    "MOS/bits/"
    "REL/bits/44"
    "OPT/bits/0"
    "DUPA/bits/0"
    "INP/"
    "INP/bits"
    "INP/bits/1"
)

for str in ${callArray[@]}; do
    echo "curl $IP/api/$str"
    # curl $IP/api/$str
    # echo ""
done

