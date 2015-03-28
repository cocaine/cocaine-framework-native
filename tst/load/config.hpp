#pragma once

#include <fstream>
#include <istream>
#include <string>

namespace testing {

namespace load {

// This magic function is required to fool the syntax parser be able to unpack the variabic pack
// without other template magic.
template<typename... Args>
static inline void ignore(Args&&...) {}

template<typename... Args>
static
void load_config(const std::string& key, Args&... args) {
    std::ifstream fstream("load.cfg");
    for (std::string line; std::getline(fstream, line);) {
        std::istringstream stream(line);
        std::string name;
        stream >> name;

        if (name == key) {
            ignore(stream >> args...);
            return;
        }
    }

    throw std::out_of_range("the key '" + key + "' was not found in config file");
}

} // namespace load

} // namespace testing
