#define GVATC_TOOL_NAME "mabootimg"
#define GVATC_VERSION "2025.11.15"

#include <fstream>
#include <cstring>
#include "argparse/argparse.hpp"
#include "termcolor/termcolor.hpp"
#include "bootimg.h"

using std::cout,std::cerr,std::endl,std::string,std::to_string,std::exception,std::function,std::string_literals::operator ""s,
      std::ifstream,std::ofstream,std::ios,std::streamsize,std::filesystem::exists,std::filesystem::file_size,
      std::array,std::vector,std::pair,std::ranges::find,std::memset,std::memcpy,std::invalid_argument,
      termcolor::bright_red,termcolor::reset,
      argparse::ArgumentParser;

constexpr char GVATC_TOOL_PRINT_PREFIX[24] = " [GVATC/" GVATC_TOOL_NAME "]: |> ";
constexpr array<unsigned,4> header_versions = {0,1,2,3,};
constexpr array<unsigned,4> page_sizes = {2048,4096,8192,16384}; // default: 4096
array<unsigned,4>::const_iterator hdr_chck2, pgs_chck;
string                            hdr_chck;

string action;
unsigned       header_version,   page_size,               os_version;
string         name,             cmdline,                 extra_cmdline,      vendor_cmdline,
               kernel_path,      ramdisk_path,            dtb_path,           vendor_ramdisk_path,
               boot_output_path, vendor_boot_output_path;
char           *kernel_data,     *ramdisk_data,           *dtb_data,          *vendor_ramdisk_data;
unsigned long  kernel_addr,      ramdisk_addr,            dtb_addr,           tags_addr,            base_addr;
size_t         name_size,        cmdline_size,            extra_cmdline_size, vendor_cmdline_size;
streamsize     kernel_size,      ramdisk_size,            dtb_size,           vendor_ramdisk_size;
vector<unsigned>   os_version_,  os_patch_level_;
constexpr unsigned v34_boot_cmdline_size = BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE;
pair<const char*,streamsize> boot_img_hdr, vendor_boot_img_hdr;

namespace print {
    void cou(const char *text) {
        std::cout << GVATC_TOOL_PRINT_PREFIX << text << std::endl;
    }
    void cer(const char *text) {
        std::cerr << bright_red << GVATC_TOOL_PRINT_PREFIX << text << std::endl;
    }
}

unsigned get_page_size_of_image(const unsigned &image_size) {
    return (image_size + page_size - 1) / page_size;
}

void set_addr(const string &addr_str,unsigned long &addr) {
    try { addr = base_addr + stoul(addr_str,nullptr,16); }
    catch (const std::invalid_argument &) {
        print::cer(("Incorrect address: "s+addr_str).c_str());
        exit(1);
    }
}

void get_file_size(const string &path,streamsize &siz) {
    siz = file_size(path);
    if (siz == 0) {
        print::cer("This file is empty.");
        exit(1);
    }
}

void set_os_version(const unsigned &major,const unsigned &minor,const unsigned &patch) { // changed SetOsVersion
    os_version &= ((1 << 11) - 1);
    os_version |= (((major & 0x7f) << 25) | ((minor & 0x7f) << 18) | ((patch & 0x7f) << 11));
}

void set_os_patch_level(unsigned &year,const unsigned &month) { // changed SetOsPatchLevel
    if (year < 2000 && month > 0) {
        print::cou("Note that OS patch level will not be specified because your year is smaller than 2000.");
        year = 0;
    } else if (month > 12) {
        print::cer("Invalid month");
        exit(1);
    } else year -= 2000;

    os_version &= ~((1 << 11) - 1);
    os_version |= ((year & 0x7f) << 4) | ((month & 0xf) << 0);
}

namespace hdr {
    void bt_chck(const ArgumentParser &args) {
        page_size = args.get<unsigned>("--page-size");
        if (pgs_chck = page_sizes.end();
            find(page_sizes.begin(),pgs_chck,page_size) == pgs_chck) {
            print::cer("Invalid or unsupported page size. Only 2048, 4096, 8192, 16384 are available.");
            exit(1);
        }

        boot_output_path = args.get<string>("--boot-output");
        if (boot_output_path.empty()) {
            print::cer("'boot' file output path must be specified.");
            exit(1);
        }

        kernel_path = args.get<string>("--kernel");
        if (kernel_path.empty()) {
            print::cer("Path to kernel file must be specified.");
            exit(1);
        }
        if (!exists(kernel_path)) {
            print::cer("Invalid kernel file path.");
            exit(1);
        }

        ramdisk_path = args.get<string>("--ramdisk");
        if (ramdisk_path.empty()) {
            print::cer("Path to ramdisk file must be specified.");
            exit(1);
        }
        if (!exists(ramdisk_path)) {
            print::cer("Invalid ramdisk file path.");
            exit(1);
        }

        print::cou("Calculating OS version value...");
        os_version_  = args.get<vector<unsigned>>("--os-version");
        if (os_version_.size() < 3) {
            print::cer("Please, specify major, minor and patch integers in OS version argument.");
            exit(1);
        }
        os_patch_level_ = args.get<vector<unsigned>>("--os-patch-level");
        if (os_patch_level_.size() < 2) {
            print::cer("Please, specify year and month integers in OS patch level argument.");
            exit(1);
        }
        os_version = 0;
        set_os_version(os_version_[0],os_version_[1],os_version_[2]);
        set_os_patch_level(os_patch_level_[0],os_patch_level_[1]);
        print::cou(to_string(os_version).c_str());

        name = args.get<string>("--name");
        name_size = name.size();
        if (name_size > 16) {
            print::cer("Length of name of product cannot be bigger than 16 characters.");
            exit(1);
        }

        cmdline = args.get<string>("--cmdline");
        cmdline_size = cmdline.size();
        if (!cmdline.empty()) {
            if (header_version < 3 && cmdline_size > BOOT_ARGS_SIZE) {
                print::cer("Length of command line before 3 header version cannot be bigger than 512 chars.");
                exit(1);
            }
            if (cmdline_size > v34_boot_cmdline_size) {
                print::cer("Length of command line in 3+ header version cannot be bigger than 1536 chars.");
                exit(1);
            }
        }

        base_addr = 0x0;
        set_addr(args.get<string>("--start-addr"),base_addr);
        set_addr(args.get<string>("--tags-addr"),tags_addr);
        set_addr(args.get<string>("--kernel-addr"), kernel_addr);
        set_addr(args.get<string>("--ramdisk-addr"),ramdisk_addr);

        print::cou("Reading kernel file...");
        get_file_size(kernel_path,kernel_size);
        kernel_data = new char[kernel_size];
        if (ifstream kernel(kernel_path,ios::binary); !kernel.read(kernel_data,kernel_size)) {
            print::cer("Failed to read this kernel file.");
            exit(1);
        }

        print::cou("Reading ramdisk file...");
        get_file_size(ramdisk_path,ramdisk_size);
        ramdisk_data = new char[ramdisk_size];
        if (ifstream ramdisk(ramdisk_path,ios::binary); !ramdisk.read(ramdisk_data,ramdisk_size)) {
            print::cer("Failed to read this ramdisk file.");
            exit(1);
        }
    }
    void xtr_cmdln(const ArgumentParser &args) {
        extra_cmdline = args.get<string>("--extra-cmdline");
        extra_cmdline_size = extra_cmdline.size();
    }
    void cv0(const ArgumentParser &args) {
        bt_chck(args);
        xtr_cmdln(args);
        //second_path =
    }
    void whdr(ofstream &writable) {
        writable.write(boot_img_hdr.first,boot_img_hdr.second);
    }
    void wbase(ofstream &writable) {
        writable.write(kernel_data,kernel_size);
        writable.write(ramdisk_data,ramdisk_size);
    }
    void wv0(ofstream &writable) {
        whdr(writable);
        wbase(writable);
        //writable.write(second_data,second_size);
    }
    pair<const char*,streamsize> v0() {
        static boot_img_hdr_v0 boot_img_hdr;
        memcpy(boot_img_hdr.magic,BOOT_MAGIC,BOOT_MAGIC_SIZE);
        boot_img_hdr.kernel_size = kernel_size;
        boot_img_hdr.kernel_addr = kernel_addr;
        boot_img_hdr.ramdisk_size = ramdisk_size;
        boot_img_hdr.ramdisk_addr = ramdisk_addr;
        boot_img_hdr.second_size = 0;
        boot_img_hdr.second_addr = 0x0;
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
        return {reinterpret_cast<const char*>(&boot_img_hdr),sizeof(boot_img_hdr)};
    }
    //void rcvrdtbo_chck() {}
    void cv1(const ArgumentParser &args) {
        cv0(args);
        //recovery_dtbo_path =
        //rcvrdtbo_chck();
    }
    void wv1(ofstream &writable) {
        wv0(writable);
        //writable.write(recovery_dtbo_data,recovery_dtbo_size);
    }
    pair<const char*,streamsize> v1() {
        static boot_img_hdr_v1 boot_img_hdr;
        memcpy(boot_img_hdr.magic,BOOT_MAGIC,BOOT_MAGIC_SIZE);
        boot_img_hdr.kernel_size = kernel_size;
        boot_img_hdr.kernel_addr = kernel_addr;
        boot_img_hdr.ramdisk_size = ramdisk_size;
        boot_img_hdr.ramdisk_addr = ramdisk_addr;
        boot_img_hdr.second_size = 0;
        boot_img_hdr.second_addr = 0x0;
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
        //boot_img_hdr.recovery_dtbo_size = recovery_dtbo_size;
        //boot_img_hdr.recovery_dtbo_offset =
        boot_img_hdr.header_size = sizeof(boot_img_hdr);
        return {reinterpret_cast<const char*>(&boot_img_hdr),sizeof(boot_img_hdr)};
    }
    void v234_dtb_chck(const ArgumentParser &args) {
        dtb_path = args.get<string>("--dtb");
        if (dtb_path.empty()) {
            print::cer("Path to DTB file must not be empty.");
            exit(1);
        }
        print::cou("Reading DTB file...");
        get_file_size(dtb_path,dtb_size);
        dtb_data = new char[dtb_size];
        if (ifstream dtb(dtb_path,ios::binary);
            !dtb.read(dtb_data,dtb_size)) {
            print::cer("Failed to read DTB file.");
            exit(1);
        }
        set_addr(args.get<string>("--dtb-addr"),dtb_addr);
    }
    void cv2(const ArgumentParser &args) {
        cv1(args);
        v234_dtb_chck(args);
    }
    void wv2(ofstream &writable) {
        wv1(writable);
        writable.write(dtb_data,dtb_size);
    }
    pair<const char*,streamsize> v2() {
        static boot_img_hdr_v2 boot_img_hdr;
        memcpy(boot_img_hdr.magic,BOOT_MAGIC,BOOT_MAGIC_SIZE);
        boot_img_hdr.kernel_size = kernel_size;
        boot_img_hdr.kernel_addr = kernel_addr;
        boot_img_hdr.ramdisk_size = ramdisk_size;
        boot_img_hdr.ramdisk_addr = ramdisk_addr;
        boot_img_hdr.second_size = 0;
        boot_img_hdr.second_addr = 0x0;
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
        //boot_img_hdr.recovery_dtbo_size = recovery_dtbo_size;
        //boot_img_hdr.recovery_dtbo_offset =
        // header_size
        boot_img_hdr.dtb_size = dtb_size;
        boot_img_hdr.dtb_addr = dtb_addr;
        boot_img_hdr.header_size = sizeof(boot_img_hdr);
        return {reinterpret_cast<const char*>(&boot_img_hdr),sizeof(boot_img_hdr)};
    }
    void vbt_chck(const ArgumentParser &args) {
        if (page_size != 4096) {
            print::cou("Note that at 3 header version page size is fixed at 4096.");
            page_size = 4096;
        }

        vendor_boot_output_path = args.get<string>("--vendor-boot-output");
        if (vendor_boot_output_path.empty()) {
            print::cer("'vendor_boot' file output path must not be empty.");
            exit(1);
        }

        vendor_ramdisk_path = args.get<string>("--vendor-ramdisk");
        if (vendor_ramdisk_path.empty()) {
            print::cer("Vendor specific ramdisk must be specified.");
            exit(1);
        } else {
            print::cou("Reading vendor ramdisk file...");
            get_file_size(vendor_ramdisk_path,vendor_ramdisk_size);
            vendor_ramdisk_data = new char[vendor_ramdisk_size];
            if (ifstream vendor_ramdisk(vendor_ramdisk_path, ios::binary);
                !vendor_ramdisk.read(vendor_ramdisk_data,vendor_ramdisk_size)) {
                print::cer("Failed to read vendor ramdisk file.");
                exit(1);
            }
        }

        v234_dtb_chck(args);

        vendor_cmdline = args.get<string>("--vendor-cmdline");
        vendor_cmdline_size = vendor_cmdline.size();

        if (vendor_cmdline_size > VENDOR_BOOT_ARGS_SIZE) {
            print::cer("Length of vendor cmdline cannot be bigger than 2048 chars.");
            exit(1);
        }
    }
    void cv3(const ArgumentParser &args) {
        vbt_chck(args);
        bt_chck(args);
    }
    void wvhdr(ofstream &writable) {
        writable.write(vendor_boot_img_hdr.first,vendor_boot_img_hdr.second);
    }
    void wv3(ofstream &writable) {
        whdr(writable);
        wbase(writable);

        ofstream vendor_boot(vendor_boot_output_path,ios::binary);
        wvhdr(vendor_boot);
        vendor_boot.write(vendor_ramdisk_data,vendor_ramdisk_size);
        vendor_boot.write(dtb_data,dtb_size);
        vendor_boot.close();
    }
    pair<const char*,streamsize> vv3() {
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
        // header_size
        boot_img_hdr.dtb_size = dtb_size;
        boot_img_hdr.dtb_addr = dtb_addr;
        boot_img_hdr.header_size = sizeof(boot_img_hdr);
        return {reinterpret_cast<const char*>(&boot_img_hdr),sizeof(boot_img_hdr)};
    }
    pair<const char*,streamsize> v3() {
        print::cou("Building 'vendor_boot' 3 header...");
        vendor_boot_img_hdr = vv3();

        static boot_img_hdr_v3 boot_img_hdr;
        memcpy(boot_img_hdr.magic,BOOT_MAGIC,BOOT_MAGIC_SIZE);
        boot_img_hdr.kernel_size = kernel_size;
        boot_img_hdr.ramdisk_size = ramdisk_size;
        boot_img_hdr.os_version = os_version;
        // header_size
        memset(boot_img_hdr.reserved,0,16);
        boot_img_hdr.header_version = header_version;
        memset(boot_img_hdr.cmdline,0,v34_boot_cmdline_size);
        memcpy(boot_img_hdr.cmdline,cmdline.c_str(),cmdline_size);
        boot_img_hdr.header_size = sizeof(boot_img_hdr);
        return {reinterpret_cast<const char*>(&boot_img_hdr),sizeof(boot_img_hdr)};
    }
}

array<function<pair<const char*,streamsize>()>,4> bldhdrs = {hdr::v0,hdr::v1,hdr::v2,hdr::v3,};
array<function<void(ofstream &)>,4> wrthdrs = {hdr::wv0,hdr::wv1,hdr::wv2,hdr::wv3,};
array<function<void(ArgumentParser &)>,4> chckhdrs = {hdr::cv0,hdr::cv1,hdr::cv2,hdr::cv3,};

int main(const int argc, const char **argv) {
    ArgumentParser parser(GVATC_TOOL_NAME,GVATC_VERSION);
    parser.add_description(GVATC_TOOL_NAME" (Manipulate Android 'bootimg') - The lightweight and fast tool to manipulate Android bootable images.");
    parser.add_epilog("Tool to create Android-specific 'boot' and 'vendor_boot' bootable images. Non-commercial use only!\n"
                        "The part of GoldenVadim's Android Tools Collection. https://goldenvadim.github.io/GVATC");

    parser.add_argument("-a","--action")
    .help("Specify the action to do with Android 'boot'")
    .metavar("<create/inspect>")
    .required();
    parser.add_argument("-H","--header-version")
    .help("[create] Specify the header version of Android 'boot'")
    .metavar("<0/1/2/3/4>")
    .default_value("");
    parser.add_argument("-p","--page-size")
    .help("[create] Specify the page size of Android 'boot'")
    .metavar("<2048/4096/8192/16384>")
    .scan<'i',unsigned>()
    .default_value(page_sizes[1]);
    parser.add_argument("-k","--kernel")
    .help("[create] Add kernel (ACK/Linux) to Android 'boot'")
    .metavar("<Image(.gz-dtb)>")
    .default_value("");
    parser.add_argument("-r","--ramdisk")
    .help("[create] Add initial RAM disk image to Android 'boot'")
    .metavar("<(Compressed) CPIO>")
    .default_value("");
    parser.add_argument("-d","--dtb")
    .help("[create] Add Device Tree Blob to Android 'boot' or 'vendor_boot'. DTB must be included in kernel file if using 0 header version")
    .metavar("<DTB>")
    .default_value("");
    parser.add_argument("-i","--vendor-ramdisk")
    .help("[create] Add vendor's specific initrd to 'vendor_boot' (3+ header version only)")
    .metavar("<(Compressed) CPIO>")
    .default_value("");
    parser.add_argument("-B","--start-addr")
    .help("[create] Use starting hexadecimal number of addresses in bootable (-B + -K/R/D/t)")
    .metavar("<0x0>")
    .default_value("0x0");
    parser.add_argument("-K","--kernel-addr")
    .help("[create] Set hexadecimal number of address of kernel image")
    .metavar("<0x0>")
    //.scan<'x',unsigned>()
    .default_value("0x0");
    parser.add_argument("-R","--ramdisk-addr")
    .help("[create] Set hexadecimal number of address of initial RAM disk(s) image(s)")
    .metavar("<0x0>")
    //.scan<'x',unsigned long>()
    .default_value("0x0");
    parser.add_argument("-D","--dtb-addr")
    .help("[create] Set hexadecimal number of address of DTB image")
    .metavar("<0x0>")
    //.scan<'x',unsigned long>()
    .default_value("0x0");
    parser.add_argument("-t","--tags-addr")
    .help("[create] Set hexadecimal number of kernel's tags if needed")
    .metavar("<0x0>")
    //.scan<'x',unsigned long>()
    .default_value("0x0");
    parser.add_argument("-n","--name")
    .help("[create] Set name of (board) product in bootable")
    .metavar("<Redmi 5>")
    .default_value("");
    parser.add_argument("-V","--os-version")
    .help("[create] Set version of operating system in bootable")
    .metavar("<0.0.0>")
    .nargs(3)
    .scan<'i',unsigned>()
    .default_value(vector<unsigned>{0,0,0});
    parser.add_argument("-P","--os-patch-level")
    .help("[create] Set patch level of operating system in bootable")
    .metavar("<0000-00>")
    .nargs(2)
    .scan<'i',unsigned>()
    .default_value(vector<unsigned>{0,0});
    parser.add_argument("-c","--cmdline")
    .help("[create] Set command line of arguments that will be given to kernel")
    .metavar("<console=tty0>")
    .default_value("");
    parser.add_argument("-C","--vendor-cmdline")
    .help("[create] Set vendor's specific command line in 'vendor_boot' (3+ header version only)")
    .metavar("<console=ttyMSM0>")
    .default_value("");
    parser.add_argument("-l","--extra-cmdline")
    .help("[create] Set additional cmdline in 'boot'. Created for compatibility with older versions of mkbootimg. Not recommended to use. (0-2 header version only)")
    .default_value("");
    parser.add_argument("-O","--vendor-boot-output")
    .help("[create] Path to output of 'vendor_boot' bootable image file")
    .metavar("<vendor_boot.img>")
    .default_value("");
    parser.add_argument("-o","--boot-output")
    .help("[create] Relative or absolute output path of 'boot' image file")
    .metavar("<boot.img>")
    .default_value("");
    parser.add_argument("-b","--boot")
    .help("[inspect] Path to Android bootable image to do with")
    .metavar("<(vendor_)boot.img>")
    .default_value("");
    parser.add_argument("-u","--unpack")
    .help("[inspect] Give kernel and ramdisk or DTB and vendor ramdisk files after giving information")
    .flag();
    parser.add_argument("--unpack-dir")
    .help("[inspect, unpack] Path to output directory of files of bootable")
    .metavar("<out>")
    .default_value("");

    try { parser.parse_args(argc,argv); }
    catch (const exception &err) {
        print::cer(err.what());
        return 1;
    }

    action = parser.get("--action");
    if (action.starts_with("i")) {
        //
        print::cou("introspection");
        //
    }else if (action.starts_with("c")) {
        print::cou("Checking arguments...");
        hdr_chck = parser.get<string>("--header-version");
        if (hdr_chck.empty()) {
            print::cer("Please, specify header version.");
            exit(1);
        }
        if (hdr_chck2 = header_versions.end();
            find(header_versions.begin(),hdr_chck2,header_version) == hdr_chck2) {
            print::cer("Invalid header version. Only 0, 1, 2, 3 and 4 are available.");
            exit(1);
        }
        header_version = stoi(hdr_chck);
        print::cou(("Header version: "s+hdr_chck).c_str());

        chckhdrs[header_version](parser);

        print::cou("Building 'boot' header...");
        boot_img_hdr = bldhdrs[header_version]();

        print::cou("Writing data...");
        ofstream boot(boot_output_path,ios::binary);
        wrthdrs[header_version](boot);

        print::cou("Successfully created bootable.");
    } else {
        print::cer("Specify create or inspect action.");
        return 1;
    }

    return 0;
}
