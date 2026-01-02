#define GVATC_TOOL_NAME    "uabootimg"
#define GVATC_TOOL_VERSION "2026.01.02"

#include <fstream>
#include <string_view>
#include "argparse/argparse.hpp"
#include "libgvatc_common_print.hpp"
#include "libgvatc_common_other.hpp"
#include "libgvatc_abootimg_os_ver_get.hpp"
#include "bootimg.h"

using argparse::ArgumentParser,std::invalid_argument,std::exception,std::to_string,std::string_view,
      std::filesystem::exists,std::filesystem::file_size,std::filesystem::path,
      std::vector,std::pair,std::ifstream,std::ofstream,std::ios,std::streamsize;

uint32_t kernel_size,    ramdisk_size,  dtb_size, vendor_ramdisk_size, second_size, header_size, recovery_dtbo_size,
         kernel_addr,    ramdisk_addr,      tags_addr,          second_addr,
         name_size,      cmdline_size,  extra_cmdline_size, vendor_cmdline_size,
         kernel_pages,   ramdisk_pages,   second_pages,  recovery_dtbo_pages,
         kernel_offset,  ramdisk_offset,  second_offset, dtb_offset,
         header_version, page_size,       os_version_patch_level, os_version, os_patch_level;
uint64_t dtb_addr, recovery_dtbo_offset;
pair<array<uint32_t,3>,array<uint32_t,2>> decoded_os_version;
string   name,           cmdline,       extra_cmdline,      vendor_cmdline;
constexpr size_t  uint32_t_size = sizeof(uint32_t);
constexpr size_t  uint64_t_size = sizeof(uint64_t);
path              image_path, directory_output_path;
vector<char>      image_data, kernel_data, ramdisk_data, cmdline_data;
streamsize        image_size;
array<char,17>    name_data;  array<char,1025> extra_cmdline_data;
array<uint32_t,9> kernel_ramdisk_second_info;
ss_necessary_manipulations ss; string_view magic;

unsigned get_number_of_pages(const unsigned &image_size) { return (image_size + page_size - 1) / page_size; }

void decode_os_version(){
    os_version = os_version_patch_level >> 11;
    os_patch_level = os_version_patch_level & ((1<<11) - 1);
    decoded_os_version = pair<array<uint32_t,3>,array<uint32_t,2>>
                         {get_os_version(os_version),get_os_patch_level(os_patch_level)};
}

namespace hdr {
    void ohdr0(){
        print::inf("Kernel size: "+         to_string(kernel_size));
        ss.hexize(kernel_addr);
        print::inf("Kernel load address: "+ ss.ss.str());
        print::inf("RAMdisk size: "+        to_string(ramdisk_size));
        ss.hexize(ramdisk_addr);
        print::inf("RAMdisk load address: "+ss.ss.str());
        print::inf("second size: "+         to_string(second_size));
        ss.hexize(second_addr);
        print::inf("second load address: "+ ss.ss.str());
        ss.hexize(tags_addr);
        print::inf("Tags load address: "+   ss.ss.str());
        print::inf("Page size: "+           to_string(page_size));
        print::inf("OS version: "+          ((decoded_os_version.first[0]!=0 || decoded_os_version.first[1]!=0 || decoded_os_version.first[2]!=0)
                                             ? to_string(decoded_os_version.first[0])+'.'
                                               +to_string(decoded_os_version.first[1])+'.'
                                               +to_string(decoded_os_version.first[2])
                                             : ""));
        print::inf("OS patch level: "+      ss.os_pl(decoded_os_version.second[0],decoded_os_version.second[1]));
        print::inf("Header version: "+      to_string(kernel_ramdisk_second_info[8]));
        print::inf("Product name: "+        name);
        print::inf("Command line: "+        cmdline);
        print::inf("Additional cmdline: "+  extra_cmdline);
    }
    void bhdr0(ifstream &image){
        kernel_size = kernel_ramdisk_second_info[0];
        kernel_addr = kernel_ramdisk_second_info[1];
        ramdisk_size = kernel_ramdisk_second_info[2];
        ramdisk_addr = kernel_ramdisk_second_info[3];
        second_size = kernel_ramdisk_second_info[4];
        second_addr = kernel_ramdisk_second_info[5];
        tags_addr = kernel_ramdisk_second_info[6];
        page_size = kernel_ramdisk_second_info[7];
        image.read(reinterpret_cast<char*>(&os_version_patch_level),uint32_t_size);
        decode_os_version();
        image.read(name_data.data(),16);
        name = name_data.data();
        cmdline_data.resize(512);
        image.seekg(32,ios::cur); // ignore SHA; instead of image.read(32)!
        image.read(extra_cmdline_data.data(),1025);
        extra_cmdline = extra_cmdline_data.data();
    }
    // ehdr0
    void ohdr1(){
        ohdr0();
        print::inf("Recovery DTBO size: "+  to_string(recovery_dtbo_size));
        ss.hexize(recovery_dtbo_offset);
        print::inf("Recovery DTBO offset: "+ss.ss.str());
        print::inf("Header size: "+         to_string(header_size));
    }
    void bhdr1(ifstream &image){
        bhdr0(image);
        image.read(reinterpret_cast<char*>(&recovery_dtbo_size),uint32_t_size);
        image.read(reinterpret_cast<char*>(&recovery_dtbo_offset),uint64_t_size);
        //image.read(reinterpret_cast<char*>(&header_size),uint32_t_size);
        header_size = BOOT_IMAGE_HEADER_V1_SIZE; // when reading from file it will be 0
    }
    // ehdr1
    void ohdr2(){
        ohdr1();
        print::inf("Device Tree Blob size: "+to_string(dtb_size));
        ss.hexize(dtb_addr);
        print::inf("DTB address: "+          ss.ss.str());
    }
    void bhdr2(ifstream &image){
        bhdr1(image);
        image.read(reinterpret_cast<char*>(&dtb_size),uint32_t_size);
        image.read(reinterpret_cast<char*>(&dtb_addr),uint64_t_size);
    }
    // ehdr2
    void ohdr3(){

    }
    void bhdr3(ifstream &image){
        kernel_size = kernel_ramdisk_second_info[0];
        ramdisk_size = kernel_ramdisk_second_info[1];
        os_version_patch_level = kernel_ramdisk_second_info[2];
        decode_os_version();
        // second_size = 0
        page_size = BOOT_IMAGE_HEADER_V34_PAGESIZE;
        cmdline_data.resize(v34_BOOT_ARGS_SIZE+1);
        image.read(cmdline_data.data(),v34_BOOT_ARGS_SIZE);
        cmdline = cmdline_data.data();
    }
    constexpr array<void(*)(ifstream&),4> bhdrs = {hdr::bhdr0,hdr::bhdr1,hdr::bhdr2,hdr::bhdr3,};
    constexpr array<void(*)(),4>          ohdrs = {hdr::ohdr0,hdr::ohdr1,hdr::ohdr2,hdr::ohdr3,};
    //constexpr array<void(*)(ifstream&),2>          ehdrs = {hdr::ehdr0,hdr::ehdr1,hdr::ehdr2,hdr::ehdr3,};
    void bhdr(ifstream &image,const ArgumentParser &args){
        image_data.resize(image_size);
        image.read(reinterpret_cast<char*>(kernel_ramdisk_second_info.data()),36);
        bhdrs[kernel_ramdisk_second_info[8]](image);
        if (!args.get<bool>("--header-only")){
            //ehdrs[kernel_ramdisk_second_info[8]]();
        }
        if (!args.get<bool>("--quiet")){
            ohdrs[kernel_ramdisk_second_info[8]]();
        }
    }
}

int main(const int argc, const char **argv) {
    ArgumentParser parser(GVATC_TOOL_NAME,GVATC_TOOL_VERSION);
    parser.add_description(GVATC_TOOL_NAME" (U.A.'bootimg') - The lightweight and fast tool to unpack Android bootable images.");
    parser.add_epilog("Tool to unpack Android-specific 'boot' and 'vendor_boot' bootable images. Non-commercial use only!\n"
                      "The part of GoldenVadim's Android Tools Collection. https://goldenvadim.github.io/GVATC");

    parser.add_argument("-i","--image")
    .help("Path to Android bootable image.")
    .metavar("<ANDROID!/VNDRBOOT>")
    .required();
    parser.add_argument("-H","--header-only")
    .help("Only give header information and don't extract files from image.")
    .flag();
    parser.add_argument("-q","--quiet")
    .help("Don't write header information to stdout from image. Conflicts with --header-only")
    .flag();
    parser.add_argument("-o","--output-dir")
    .help("Path to output directory of files used in image (kernel, ramdisk, dtb, bootconfig, recovery dtbo and second)")
    .metavar("<DIR>")
    .required();

    try { parser.parse_args(argc,argv); }
    catch (const exception &err) {
        print::err(err.what());
        return 1;
    }

    print::inf("Checking arguments...");
    image_path = parser.get<string>("--image");
    if (!exists(image_path)){
        print::err("Invalid bootable image path.");
        return 1;
    }

    print::inf("Reading image...");
    image_size = file_size(image_path);
    if (image_size == 0) {
        print::err("This file is empty.");
        return 1;
    }
    ifstream image(image_path,ios::binary);
    if (!image.is_open()) {
        print::err("Failed to open this file.");
        return 1;
    }
    image_data.resize(BOOT_MAGIC_SIZE);
    image.read(image_data.data(),BOOT_MAGIC_SIZE);
    magic = string_view(image_data.data(),BOOT_MAGIC_SIZE);
    if (magic==BOOT_MAGIC){
        hdr::bhdr(image,parser);
    } /*else if (magic==VENDOR_BOOT_MAGIC){
        hdr::vbhdr(image,parser);
    }*/else {
        print::err("Invalid magic.");
        return 1;
    }

    return 0;
}