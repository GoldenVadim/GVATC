#include <sstream>
#include <cstdint>
using std::stringstream;

class ss_necessary_manipulations{
public:
    stringstream ss;
    void hexize(int dec);
    string os_pl(const uint32_t &y,const uint32_t &m);
};
