#include <iostream>
#include <cstdint>
#include "termcolor/termcolor.hpp"
#include "libgvatc_common.h"

using termcolor::bright_red,termcolor::reset,termcolor::bright_yellow;

namespace print {
    void inf(const string &text) {
        std::cout << " (i)> |: " << text << std::endl;
    }
    void wrn(const string &text) {
        std::cout << bright_yellow << " (!)> |: " << text << reset << std::endl;
    }
    void err(const string &text) {
        std::cerr << bright_red << " (X)> |: " << text << std::endl;
    }
}