#include <iostream>
#include <map>
#include <stdio.h>

#include <toml.hpp> 

#define ALOGT(...) std::cout << (__VA_ARGS__) << std::endl;
#define ALOGD(...) std::cout << (__VA_ARGS__) << std::endl;
#define ALOGI(...) std::cout << (__VA_ARGS__) << std::endl;
#define ALOGW(...) std::cout << (__VA_ARGS__) << std::endl;
#define ALOGE(...) std::cout << (__VA_ARGS__) << std::endl;
#define ALOGV(...) std::cout << (__VA_ARGS__) << std::endl;

#include "configHandler.h"


int main(int argc, char* argv[]){
    std::ifstream file( argv[1] );
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::istringstream is(buffer.str());

    Config.parseToml(is, argv[1]);
    Config.printConfig();
    return 0;
}






// void parse_config(std::string name){
//     const auto data  = toml::parse(name.c_str());
//     const auto title = toml::find<std::string>(data, "version");
//     const auto values = toml::find(data, "pin");

//     for(const auto& v : values.as_array())
//     {
//         pin_t pin = {
//             .name = toml::find<std::string>(v,"name"),
//             .antctrl = toml::find<std::string>(v,"antctrl"),
//             .sch = toml::find<std::string>(v,"sch")
//         };        
//         std::cout << pin.to_string() << std::endl;
//     }

//     const auto button_groups = toml::find(data, "buttons");
//     std::map<std::string, std::vector<button_t>> button_map;

//     for(const auto& bg : button_groups.as_table()){
//         button_map[bg.first] = {};
//         for(const auto& b : bg.second.as_array()){
//             button_t button = {
//                 .name = toml::find<std::string>(b,"name"),
//                 .pins = toml::find<std::vector<std::string>>(b,"pins")
//             };
//             button_map[bg.first].push_back(button);
//         }
//     }    

//     for (auto& b_group : button_map) {
//         std::cout << b_group.first << ":\n";
//         for(auto& b : b_group.second){
//             std::cout << b.to_string() << std::endl;
//         }
//     }

// }