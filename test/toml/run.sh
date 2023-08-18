
rm -f toml.out
TOML_DIR=../../.pio/libdeps/antcontroller-local/toml11
# FMT_DIR=../../.pio/libdeps/antcontroller-local/fmt/include/fmt
# CONF_DIR=../../data/config.toml

g++ -DFMT_HEADER_ONLY -I$TOML_DIR -I -I/usr/include/ -I../../include tomltest.cpp -o toml.out



# ./toml.out test_configs/config_valid.toml
# ./toml.out test_configs/empty.toml
# ./toml.out test_configs/cut.toml
# ./toml.out test_configs/invalid_keys.toml
./toml.out test_configs/invalid_pin_names.toml

