#include <array>
#include <cstdint>
using std::array;
array<uint32_t,3> get_os_version(uint32_t &os_version);
array<uint32_t,2> get_os_patch_level(uint32_t &os_version);