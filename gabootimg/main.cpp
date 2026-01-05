#define GVATC_TOOL_NAME "gabootimg (G.A.'bootimg')"

#include <fstream>
#include <cstring>
/*#include <openssl/evp.h>
#include <openssl/err.h>*/
#include <argparse/argparse.hpp>
#include "../gvatc.hpp"
#include "../libgvatc/common/print.hpp"
#include "../libgvatc/abootimg/os_ver_set.hpp"
#include "../libgvatc/abootimg/pages.hpp"
#include "bootimg.h"

using std::exception,std::function,std::stoi,std::hex,std::to_string,
      std::array,std::vector,std::pair,std::find,std::memset,std::memcpy,
      std::filesystem::exists,std::filesystem::file_size,std::filesystem::path,
      std::ifstream,std::ofstream,std::ios,
      argparse::ArgumentParser,std::invalid_argument;

constexpr array<uint32_t,4> header_versions = {0,1,2,3,};
constexpr array<uint32_t,4> page_sizes = {2048,4096,8192,16384};

path         boot_output_path, vendor_boot_output_path,
             kernel_path,      ramdisk_path,            dtb_path,           vendor_ramdisk_path, recovery_dtbo_path, second_path;
vector<char> kernel_data,      ramdisk_data,            dtb_data,           vendor_ramdisk_data, recovery_dtbo_data, second_data, pad;
uint32_t     kernel_size,      ramdisk_size,            dtb_size,           vendor_ramdisk_size, recovery_dtbo_size, second_size,
             kernel_addr,      ramdisk_addr,            tags_addr,           /*ramdisk_addr,*/   recovery_dtbo_addr, second_addr, base_addr,
             name_size,        cmdline_size,            extra_cmdline_size, vendor_cmdline_size, pad_size,
             header_version,   page_size,               os_version;         uint64_t dtb_addr; //unsigned char id[EVP_MAX_MD_SIZE];
string       name,             cmdline,                 extra_cmdline,      vendor_cmdline,      action;
pair<const char*,size_t> boot_img_hdr,     vendor_boot_img_hdr;
vector<uint32_t>         os_version_,      os_patch_level_;
vector<ofstream>         writables;

void pad_file(ofstream &file) { // I DONT LIKE IT
    pad_size = (page_size - (file.tellp() & (page_size - 1))) & (page_size - 1);
    pad.resize(pad_size);
    file.write(pad.data(),pad_size);
}

void set_addr(const string &addr_str,unsigned &addr) {
    try { addr = base_addr + stoi(addr_str,nullptr,16); }
    catch (const std::invalid_argument &) {
        print::err("Incorrect address number: "+addr_str);
        exit(1);
    }
}

uint64_t get_recovery_dtbo_offset(){
    number_of_pages pages(page_size);
    return page_size * (1 + pages.get(kernel_size)
                          + pages.get(ramdisk_size)
                          + pages.get(second_size));
}

void read_file(const path &path,const string &what,uint32_t &size,vector<char> &buffer) {
    print::inf("Reading "+what+" file...");
    size = file_size(path);
    if (size == 0) {
        print::err("This file is empty.");
        exit(1);
    }
    ifstream file(path,ios::binary);
    if (!file.is_open()) {
        print::err("Failed to open this "+what+" file.");
        exit(1);
    }
    buffer.resize(size);
    if (!file.read(buffer.data(),size)) {
        print::err("Failed to read this "+what+" file.");
        exit(1);
    }
}

namespace hdr {
    void bt_chck(const ArgumentParser &args) {
        boot_output_path = args.get<string>("--boot-output");
        if (boot_output_path.empty()) {
            print::err("'boot' file output path must be specified.");
            exit(1);
        }

        kernel_path = args.get<string>("--kernel");
        if (kernel_path.empty()) {
            print::err("Path to kernel file must be specified.");
            exit(1);
        }
        if (!exists(kernel_path)) {
            print::err("Invalid kernel file path.");
            exit(1);
        }

        ramdisk_path = args.get<string>("--ramdisk");
        if (ramdisk_path.empty()) {
            print::err("Path to ramdisk file must be specified.");
            exit(1);
        }
        if (!exists(ramdisk_path)) {
            print::err("Invalid ramdisk file path.");
            exit(1);
        }

        print::inf("Calculating OS version value...");
        os_version_ = args.get<vector<uint32_t>>("--os-version");
        if (os_version_.size() < 3) {
            print::err("Please, specify major, minor and patch integers in OS version argument.");
            exit(1);
        }
        os_patch_level_ = args.get<vector<uint32_t>>("--os-patch-level");
        if (os_patch_level_.size() < 2) {
            print::err("Please, specify year and month integers in OS patch level argument.");
            exit(1);
        }
        set_os_version(os_version,os_version_[0],os_version_[1],os_version_[2]);
        set_os_patch_level(os_version,os_patch_level_[0],os_patch_level_[1]);

        name = args.get<string>("--name");
        name_size = name.size();
        if (name_size > BOOT_NAME_SIZE) {
            print::err("Length of name of product cannot be bigger than 16 characters.");
            exit(1);
        }

        cmdline = args.get<string>("--cmdline");
        cmdline_size = cmdline.size();
        if (!cmdline.empty()) {
            if (header_version < 3 && cmdline_size > BOOT_ARGS_SIZE) {
                print::err("Length of command line in 'boot' before 3 header version cannot be bigger than 512 chars.");
                exit(1);
            }
            if (cmdline_size > v34_BOOT_ARGS_SIZE) {
                print::err("Length of command line in 'boot' in 3+ header version cannot be bigger than 1536 chars.");
                exit(1);
            }
        }

        base_addr = 0x0;
        set_addr(args.get<string>("--start-addr"),base_addr);
        set_addr(args.get<string>("--tags-addr"),tags_addr);
        set_addr(args.get<string>("--kernel-addr"), kernel_addr);
        set_addr(args.get<string>("--ramdisk-addr"),ramdisk_addr);
    }
//// Check stage
    void cv0(const ArgumentParser &args) {
        page_size = args.get<uint32_t>("--page-size");
        if (find(page_sizes.begin(),page_sizes.end(),page_size) == page_sizes.end()) {
            print::err("Invalid or unsupported page size. Only 2048, 4096, 8192, 16384 are available.");
            exit(1);
        }
        bt_chck(args);
        extra_cmdline = args.get<string>("--extra-cmdline");
        extra_cmdline_size = extra_cmdline.size();
        second_path = args.get<string>("--second");
        if (!second_path.empty()){
            if (!exists(second_path)){
                print::err("Invalid second bootloader file path.");
                exit(1);
            } else set_addr(args.get<string>("--second-addr"),second_addr);
        } 
    }
//// Read stage
    void rd_base() {
        read_file(kernel_path,"kernel",kernel_size,kernel_data);
        read_file(ramdisk_path,"ramdisk",ramdisk_size,ramdisk_data);
    }
    void rv0() {
        rd_base();
        if (!second_path.empty()) read_file(second_path,"second",second_size,second_data);
    }
//// Open stage
    void ov0() {
        writables.emplace_back(boot_output_path,ios::binary);
    }
//// Write stage  
    void wboot(ofstream &writable) {
        // hdr
        writable.write(boot_img_hdr.first,boot_img_hdr.second);
        pad_file(writable);
        // base
        writable.write(kernel_data.data(),kernel_size);
        pad_file(writable);
        writable.write(ramdisk_data.data(),ramdisk_size);
        pad_file(writable);
    }
    void wv0() {
        wboot(writables[0]);
        writables[0].write(second_data.data(),second_size);
        pad_file(writables[0]);
    }
    pair<const char*,size_t> v0() {
        static boot_img_hdr_v0 boot_img_hdr;
        memcpy(boot_img_hdr.magic,BOOT_MAGIC,BOOT_MAGIC_SIZE);
        boot_img_hdr.kernel_size = kernel_size;
        boot_img_hdr.kernel_addr = kernel_addr;
        boot_img_hdr.ramdisk_size = ramdisk_size;
        boot_img_hdr.ramdisk_addr = ramdisk_addr;
        boot_img_hdr.second_size = second_size;
        boot_img_hdr.second_addr = second_addr;
        boot_img_hdr.tags_addr = tags_addr;
        boot_img_hdr.page_size = page_size;
        boot_img_hdr.header_version = header_version;
        boot_img_hdr.os_version = os_version;
        memset(boot_img_hdr.name,0,BOOT_NAME_SIZE);
        memcpy(boot_img_hdr.name,name.c_str(),name_size);
        memset(boot_img_hdr.cmdline, 0,BOOT_ARGS_SIZE);
        memcpy(boot_img_hdr.cmdline,cmdline.c_str(),cmdline_size);
        // boot_img_hdr.id
        memset(boot_img_hdr.extra_cmdline,0,BOOT_EXTRA_ARGS_SIZE);
        memcpy(boot_img_hdr.extra_cmdline,extra_cmdline.c_str(),extra_cmdline_size);
        return {reinterpret_cast<const char*>(&boot_img_hdr),sizeof(boot_img_hdr)};
    }
//// Check stage
    void cv1(const ArgumentParser &args) {
        cv0(args);
        recovery_dtbo_path = args.get<string>("--recovery-dtbo");
        if (!recovery_dtbo_path.empty() && !exists(recovery_dtbo_path)) {
            print::err("Invalid recovery DTBO file path.");
            exit(1);
        }
    }
//// Read stage
    void rv1() {
        rv0();
        if (!recovery_dtbo_path.empty()) read_file(recovery_dtbo_path,"recovery DTBO",recovery_dtbo_size,recovery_dtbo_data);
    }
//// Open stage 
    void ov1() {
        ov0();
    }
//// Write stage 
    void wv1() {
        wv0();
        writables[0].write(recovery_dtbo_data.data(),recovery_dtbo_size);
        pad_file(writables[0]);
    }
    pair<const char*,size_t> v1() {
        static boot_img_hdr_v1 boot_img_hdr;
        memcpy(boot_img_hdr.magic,BOOT_MAGIC,BOOT_MAGIC_SIZE);
        boot_img_hdr.kernel_size = kernel_size;
        boot_img_hdr.kernel_addr = kernel_addr;
        boot_img_hdr.ramdisk_size = ramdisk_size;
        boot_img_hdr.ramdisk_addr = ramdisk_addr;
        boot_img_hdr.second_size = second_size;
        boot_img_hdr.second_addr = second_addr;
        boot_img_hdr.tags_addr = tags_addr;
        boot_img_hdr.page_size = page_size;
        boot_img_hdr.header_version = header_version;
        boot_img_hdr.os_version = os_version;
        memset(boot_img_hdr.name,0,BOOT_NAME_SIZE);
        memcpy(boot_img_hdr.name,name.c_str(),name_size);
        memset(boot_img_hdr.cmdline,0,BOOT_ARGS_SIZE);
        memcpy(boot_img_hdr.cmdline,cmdline.c_str(),cmdline_size);
        //boot_img_hdr.id = id;
        memset(boot_img_hdr.extra_cmdline,0,BOOT_EXTRA_ARGS_SIZE);
        memcpy(boot_img_hdr.extra_cmdline,extra_cmdline.c_str(),extra_cmdline_size);
        boot_img_hdr.recovery_dtbo_size = recovery_dtbo_size;
        boot_img_hdr.recovery_dtbo_offset = get_recovery_dtbo_offset();
        boot_img_hdr.header_size = BOOT_IMAGE_HEADER_V1_SIZE;
        return {reinterpret_cast<const char*>(&boot_img_hdr),sizeof(boot_img_hdr)};
    }
//// Check stage
    void v234_dtb_chck(const ArgumentParser &args) {
        dtb_path = args.get<string>("--dtb");
        if (dtb_path.empty()) {
            print::err("Path to DTB file must not be empty.");
            exit(1);
        }
        if (!exists(dtb_path)) {
            print::err("Invalid DTB file path.");
            exit(1);
        }
        dtb_addr = stoi(args.get("--dtb-addr"),nullptr,16); // cant use set_addr
    }
    void cv2(const ArgumentParser &args) {
        cv1(args);
        v234_dtb_chck(args);
    }
//// Read stage
    void v234_rd_dtb() {
        read_file(dtb_path,"DTB",dtb_size,dtb_data);
    }
    void rv2() {
        rv1();
        v234_rd_dtb();
    }
//// Open stage
    void ov2() {
        ov1();
    }
//// Write stage
    void v234_wrt_dtb(ofstream &writable) {
        writable.write(dtb_data.data(),dtb_size);
        pad_file(writable);
    }
    void wv2() {
        wv1();
        v234_wrt_dtb(writables[0]);
    }
    pair<const char*,size_t> v2() {
        static boot_img_hdr_v2 boot_img_hdr;
        memcpy(boot_img_hdr.magic,BOOT_MAGIC,BOOT_MAGIC_SIZE);
        boot_img_hdr.kernel_size = kernel_size;
        boot_img_hdr.kernel_addr = kernel_addr;
        boot_img_hdr.ramdisk_size = ramdisk_size;
        boot_img_hdr.ramdisk_addr = ramdisk_addr;
        boot_img_hdr.second_size = second_size;
        boot_img_hdr.second_addr = second_addr;
        boot_img_hdr.tags_addr = tags_addr;
        boot_img_hdr.page_size = page_size;
        boot_img_hdr.header_version = header_version;
        boot_img_hdr.os_version = os_version;
        memset(boot_img_hdr.name,0,BOOT_NAME_SIZE);
        memcpy(boot_img_hdr.name,name.c_str(),name_size);
        memset(boot_img_hdr.cmdline, 0,BOOT_ARGS_SIZE);
        memcpy(boot_img_hdr.cmdline,cmdline.c_str(),cmdline_size);
        //boot_img_hdr.id = id;
        memset(boot_img_hdr.extra_cmdline, 0,BOOT_EXTRA_ARGS_SIZE);
        memcpy(boot_img_hdr.extra_cmdline,extra_cmdline.c_str(),extra_cmdline_size);
        boot_img_hdr.recovery_dtbo_size = recovery_dtbo_size;
        boot_img_hdr.recovery_dtbo_offset = get_recovery_dtbo_offset();
        boot_img_hdr.header_size = BOOT_IMAGE_HEADER_V2_SIZE;
        boot_img_hdr.dtb_size = dtb_size;
        boot_img_hdr.dtb_addr = dtb_addr;
        return {reinterpret_cast<const char*>(&boot_img_hdr),sizeof(boot_img_hdr)};
    }
    void vbt_chck(const ArgumentParser &args) {
        if (page_size != BOOT_IMAGE_HEADER_V34_PAGESIZE) {
            print::wrn("Note that at 3 header version page size is fixed at 4096.");
            page_size = 4096;
        }

        vendor_boot_output_path = args.get<string>("--vendor-boot-output");
        if (vendor_boot_output_path.empty()) {
            print::err("'vendor_boot' file output path must not be empty.");
            exit(1);
        }

        vendor_ramdisk_path = args.get<string>("--vendor-ramdisk");
        if (vendor_ramdisk_path.empty()) {
            print::err("Vendor specific ramdisk must be specified.");
            exit(1);
        }
        if (!exists(vendor_ramdisk_path)) {
            print::err("Invalid vendor ramdisk path.");
            exit(1);
        }

        v234_dtb_chck(args);

        vendor_cmdline = args.get<string>("--vendor-cmdline");
        vendor_cmdline_size = vendor_cmdline.size();

        if (vendor_cmdline_size > VENDOR_BOOT_ARGS_SIZE) {
            print::err("Length of vendor cmdline cannot be bigger than 2048 chars.");
            exit(1);
        }
    }
//// Check stage
    void cv3(const ArgumentParser &args) {
        bt_chck(args);
        vbt_chck(args);
    }
//// Read stage
    void rv3() {
        rd_base();
        read_file(vendor_ramdisk_path,"vendor ramdisk",vendor_ramdisk_size,vendor_ramdisk_data);
        v234_rd_dtb();
    }
//// Open stage
    void ovboot(){
        writables.emplace_back(vendor_boot_output_path,ios::binary);
    }
    void ov3() {
        ov0();
        ovboot();
    }
//// Write stage
    void wvboot(ofstream &writable) {
        // vhdr
        writable.write(vendor_boot_img_hdr.first,vendor_boot_img_hdr.second);
        pad_file(writable);
        // vbase
        writable.write(vendor_ramdisk_data.data(),vendor_ramdisk_size);
        pad_file(writable);
        v234_wrt_dtb(writables[1]);
    }
    void wv3() {
        // boot
        wboot(writables[0]);
        // vendor_boot
        wvboot(writables[1]);
    }
    pair<const char*,size_t> vhdr(const function<pair<const char*,size_t>()> &vhdr){ // looks studip but ok
        print::inf("Building 'vendor_boot' header...");
        return vhdr();
    }
    pair<const char*,size_t> vv3() {
        static vendor_boot_img_hdr_v3 boot_img_hdr;
        memcpy(boot_img_hdr.magic,VENDOR_BOOT_MAGIC,VENDOR_BOOT_MAGIC_SIZE);
        boot_img_hdr.header_version = header_version;
        boot_img_hdr.page_size = page_size;
        boot_img_hdr.kernel_addr = kernel_addr;
        boot_img_hdr.ramdisk_addr = ramdisk_addr;
        boot_img_hdr.vendor_ramdisk_size = vendor_ramdisk_size;
        memset(boot_img_hdr.cmdline, 0,BOOT_ARGS_SIZE);
        memcpy(boot_img_hdr.cmdline,vendor_cmdline.c_str(),vendor_cmdline_size);
        //boot_img_hdr.id = id;
        boot_img_hdr.tags_addr = tags_addr;
        memset(boot_img_hdr.name,0,BOOT_NAME_SIZE);
        memcpy(boot_img_hdr.name,name.c_str(),name_size);
        boot_img_hdr.header_size = VENDOR_BOOT_IMAGE_HEADER_V3_SIZE;
        boot_img_hdr.dtb_size = dtb_size;
        boot_img_hdr.dtb_addr = dtb_addr;
        return {reinterpret_cast<const char*>(&boot_img_hdr),sizeof(boot_img_hdr)};
    }
    pair<const char*,size_t> v3() {
        vendor_boot_img_hdr = vhdr(vv3);

        static boot_img_hdr_v3 boot_img_hdr;
        memcpy(boot_img_hdr.magic,BOOT_MAGIC,BOOT_MAGIC_SIZE);
        boot_img_hdr.kernel_size = kernel_size;
        boot_img_hdr.ramdisk_size = ramdisk_size;
        boot_img_hdr.os_version = os_version;
        boot_img_hdr.header_size = BOOT_IMAGE_HEADER_V3_SIZE;
        memset(boot_img_hdr.reserved,0,16);
        boot_img_hdr.header_version = header_version;
        memset(boot_img_hdr.cmdline,0,v34_BOOT_ARGS_SIZE);
        memcpy(boot_img_hdr.cmdline,cmdline.c_str(),cmdline_size);
        return {reinterpret_cast<const char*>(&boot_img_hdr),sizeof(boot_img_hdr)};
    }
}

constexpr array<pair<const char*,std::size_t>(*)(),4> bldhdrs = {hdr::v0, hdr::v1, hdr::v2, hdr::v3};
constexpr array<void(*)(const ArgumentParser&),4> chckhdrs = {hdr::cv0, hdr::cv1, hdr::cv2, hdr::cv3};
using nortrnfnc = array<void(*)(),4>;
constexpr nortrnfnc rdhdrs = {hdr::rv0,hdr::rv1,hdr::rv2,hdr::rv3,};
constexpr nortrnfnc opnfls = {hdr::ov0,hdr::ov1,hdr::ov2,hdr::ov3,};
constexpr nortrnfnc wrthdrs = {hdr::wv0,hdr::wv1,hdr::wv2,hdr::wv3,};

int main(const int argc, char* const argv[]){
    ArgumentParser parser(GVATC_TOOL_NAME,GVATC_VERSION);
    parser.add_description(GVATC_TOOL_NAME" - The lightweight and fast tool to generate Android bootable images.");
    parser.add_epilog("Tool to create Android-specific 'boot' and 'vendor_boot' bootable images. Non-commercial use only!\n"
                      "The part of GoldenVadim's Android Tools Collection. https://goldenvadim.github.io/GVATC");

    parser.add_argument("-H","--header-version")
    .help("Specify the header version of Android bootable image(s)")
    .metavar("<0/1/2/3/4>")
    .scan<'i',unsigned>()
    .required();
    parser.add_argument("-p","--page-size")
    .help("Specify the page size in Android bootable image")
    .metavar("<2048/4096/8192/16384>")
    .scan<'i',unsigned>()
    .required();
    parser.add_argument("-k","--kernel")
    .help("Add kernel (ACK/Linux) to Android 'boot'")
    .metavar("<Image(.gz-dtb)>")
    .default_value("");
    parser.add_argument("-s","--second")
    .help("Add optional secondary bootloader to 'boot'. Only for header versions before 3")
    .metavar("<...>")
    .default_value("");
    parser.add_argument("-r","--ramdisk")
    .help("Add initial RAM filesystem image to Android 'boot'")
    .metavar("<(Compressed) CPIO>")
    .default_value("");
    parser.add_argument("-d","--dtb")
    .help("Add Device Tree Blob to Android 'boot' or 'vendor_boot'. DTB must be included in kernel file if using 0 header version")
    .metavar("<DTB>")
    .default_value("");
    parser.add_argument("--recovery-dtbo")
    .help("Add optional recovery DTBO to 'boot'. Only for 1 & 2 header versions. The offset (or load address) of it will be calculated")
    .metavar("<DTBO>")
    .default_value("");
    parser.add_argument("-i","--vendor-ramdisk")
    .help("Add vendor's initial RAM filesystem image to 'vendor_boot' (3+ header version only)")
    .metavar("<(Compressed) CPIO>")
    .default_value("");
    parser.add_argument("-B","--start-addr")
    .help("Use load addresses arguments as offsets (-B + -K/R/D/t)")
    .metavar("<0x0>")
    .default_value("0x10000000");
    parser.add_argument("-K","--kernel-addr")
    .help("Set hexadecimal number of load address of kernel image")
    .metavar("<0x0>")
    .default_value("0x00008000");
    parser.add_argument("-R","--ramdisk-addr")
    .help("Set hexadecimal number of load address of ramdisk(s) image(s)")
    .metavar("<0x0>")
    .default_value("0x01000000");
    parser.add_argument("-S","--second-addr")
    .help("Set hexadecimal number of load address of second bootloader")
    .metavar("<0x0>")
    .default_value("0x00f00000");
    parser.add_argument("-D","--dtb-addr")
    .help("Set hexadecimal number of load address of Device Tree Blob")
    .metavar("<0x0>")
    .default_value("0x01f00000");
    parser.add_argument("-t","--tags-addr")
    .help("Set hexadecimal number of load address of kernel's tags")
    .metavar("<0x0>")
    .default_value("0x00000100");
    parser.add_argument("-n","--name")
    .help("Set name of product (board) in bootable image")
    .metavar("<Redmi 5>")
    .default_value("");
    parser.add_argument("-V","--os-version")
    .help("Set version of operating system in bootable")
    .metavar("<0.0.0>")
    .nargs(3)
    .scan<'i',unsigned>()
    .default_value(vector<uint32_t>{0,0,0});
    parser.add_argument("-P","--os-patch-level")
    .help("Set patch level of operating system in bootable")
    .metavar("<0000-00>")
    .nargs(2)
    .scan<'i',unsigned>()
    .default_value(vector<uint32_t>{0,0});
    parser.add_argument("-c","--cmdline")
    .help("Set command line of arguments that will be given to kernel")
    .metavar("<console=tty0>")
    .default_value("");
    parser.add_argument("-C","--vendor-cmdline")
    .help("Set vendor's command line in 'vendor_boot' (3+ header version only)")
    .metavar("<console=ttyMSM0>")
    .default_value("");
    parser.add_argument("-l","--extra-cmdline")
    .help("Set additional cmdline in 'boot'. Created for compatibility with older versions of mkbootimg. Not recommended to use. 0-2 header versions only")
    .default_value("");
    parser.add_argument("-O","--vendor-boot-output")
    .help("Path to output of 'vendor_boot' bootable image file")
    .metavar("<vendor_boot.img>")
    .default_value("");
    parser.add_argument("-o","--boot-output")
    .help("Relative or absolute output path of 'boot' image file")
    .metavar("<boot.img>")
    .required();
    /*parser.add_argument("--print-id")
    .help("Print the generated ID (SHA1 checksum) of bootable")
    .flag();*/

    try { 
        parser.parse_args(argc,argv);
    
        print::inf("Checking arguments...");
        header_version = parser.get<unsigned>("--header-version");
        if (header_version > header_versions.size()-1) {
            print::err("Invalid header version. Only 0, 1, 2, 3 are available.");
            return 1;
        }
        print::inf("Header version: "+to_string(header_version));

        chckhdrs[header_version](parser);

        print::inf("Reading required files...");
        rdhdrs[header_version]();

        print::inf("Building 'boot' header...");
        boot_img_hdr = bldhdrs[header_version]();

        print::inf("Writing data...");
        opnfls[header_version]();
        wrthdrs[header_version]();
    }
    catch (const exception &err) {
        print::err(err.what());
        return 1;
    }

    print::inf("Successfully created bootable.");
    return 0;
}