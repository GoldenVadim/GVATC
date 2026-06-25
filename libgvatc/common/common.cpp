#include <iostream>
#include <iomanip>
#if defined(_WIN32)
#define NOMINMAX
#endif
#include "print.hpp"
#include "other.hpp"

using std::setfill,std::setw,std::hex,std::dec;

namespace print {
    void inf(const string &text) {
        std::cout << " (i)> |: " << text << std::endl;
    }
    void wrn(const string &text) {
        std::cout << "\033[93" << " (!)> |: " << text << std::endl;
    }
    void err(const string &text) {
        std::cerr << "\033[91" << " (X)> |: " << text << std::endl;
    }
}

void ss_necessary_manipulations::hexize(unsigned long long dec){
    ss.str("");
    ss.clear();
    ss << "0x" << hex << dec;
}

string ss_necessary_manipulations::os_pl(const unsigned int &y,const unsigned int &m){
    ss.str("");
    ss.clear();
    if (y >= 2000 && m > 0) ss << dec << setw(4) << setfill('0') << y << '-' << setw(2) << setfill('0') << m;
    return ss.str();
}