#include <sstream>
using std::stringstream;

class ss_necessary_manipulations{
public:
    stringstream ss;
    void hexize(unsigned long long dec);
    string os_pl(const unsigned int &y,const unsigned int &m);
};
