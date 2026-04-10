#include "../common/print.hpp"
#include "pages.hpp"
#include "os_ver_set.hpp"
#include "os_ver_get.hpp"

void set_os_version(unsigned int &os_version,const unsigned int &major,const unsigned int &minor,const unsigned int &patch) { // changed SetOsVersion
    os_version &= ((1 << 11) - 1);
    os_version |= (((major & 0x7f) << 25) | ((minor & 0x7f) << 18) | ((patch & 0x7f) << 11));
}

void set_os_patch_level(unsigned int &os_version,unsigned int &year,const unsigned int &month) { // changed SetOsPatchLevel
    if (month > 12) {
        print::err("Invalid month");
        exit(1);
    }
    if (month == 0) year = 0;
    else if (year < 2000) {
        print::wrn("Note that year in OS patch level will be 2000");
        year = 0;
    } else year -= 2000;

    os_version &= ~((1 << 11) - 1);
    os_version |= ((year & 0x7f) << 4) | ((month & 0xf) << 0);
}

unsigned int major,minor,patch,
            year, month;

array<unsigned int,3> get_os_version(const unsigned int &os_version){
    major = os_version >> 14;
    minor = os_version >> 7 & ((1<<7) - 1);
    patch = os_version & ((1<<7) - 1);
    return array<unsigned int,3>{major,minor,patch};
}

array<unsigned int,2> get_os_patch_level(const unsigned int &os_version){
    year = os_version >> 4;
    year += 2000;
    month = os_version & ((1<<4) - 1);
    return array<unsigned int,2>{year,month};
}

unsigned number_of_pages::get(const unsigned &image_size) const {
    return (image_size + page_size - 1) / page_size;
}
