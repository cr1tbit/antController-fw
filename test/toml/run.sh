
rm -f toml.out
TOML_DIR=../../.pio/libdeps/antcontroller-local/toml11
# CONF_DIR=../../data/config.toml

g++ -std=c++11 -I$TOML_DIR -I../../include tomltest.cpp -o toml.out



./toml.out test_configs/config_valid.toml
# ./toml.out test_configs/empty.toml
# ./toml.out test_configs/cut.toml
# ./toml.out test_configs/invalid_keys.toml

