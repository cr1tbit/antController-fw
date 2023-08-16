
rm -f toml.out
TOML_DIR=../../.pio/libdeps/antcontroller-local/toml11
CONF=../../data/config.toml

g++ -std=c++11 -I$TOML_DIR $TOML_DIR/tests/check_toml_test.cpp -o toml.out

cat $CONF | ./toml.out | jq > result.json

