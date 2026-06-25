#define GVATC_TOOL_NAME "uabootimg (U.A.'bootimg')"

#include <fstream>
#include <string_view>
#include <unordered_map>
#include <argparse/argparse.hpp>
#include "../libgvatc/gvatc.hpp"
#include "../libgvatc/common/print.hpp"
#include "../libgvatc/common/other.hpp"
#include "../libgvatc/abootimg/pages.hpp"
#include "../libgvatc/abootimg/os_ver_get.hpp"
#include "../libgvatc/abootimg/bootimg.h"

using argparse::ArgumentParser,std::invalid_argument,std::exception,std::to_string,std::string_view,
      std::filesystem::exists,std::filesystem::file_size,std::filesystem::path,std::filesystem::create_directories,
      std::vector,std::pair,std::unordered_map,
      std::ifstream,std::ofstream,std::ios,std::streamsize;

ArgumentParser parser(GVATC_TOOL_NAME,GVATC_VERSION);
ifstream image;
unsigned int kernel_size,    ramdisk_size,   dtb_size, vendor_ramdisk_size, second_size, header_size, recovery_dtbo_size,
             kernel_addr,    ramdisk_addr,      tags_addr,          second_addr,
             name_size,      cmdline_size,   extra_cmdline_size, vendor_cmdline_size,
             kernel_pages,   ramdisk_pages,  second_pages,  recovery_dtbo_pages,
             kernel_offset,  ramdisk_offset, second_offset, dtb_offset,
             header_version, page_size,      os_version_patch_level, os_version, os_patch_level;
unsigned long long dtb_addr, recovery_dtbo_offset;
pair<array<unsigned int,3>,array<unsigned int,2>> decoded_os_version;
string                     name,           cmdline,       extra_cmdline,      vendor_cmdline, args;// sha
path                       image_path, directory_output_path;
vector<char>      image_data, buffer, cmdline_data;
streamsize        image_size;
array<char,BOOT_NAME_SIZE>       name_data;
array<char,BOOT_EXTRA_ARGS_SIZE> extra_cmdline_data; 
//array<char,32> sha_data;
array<unsigned,9> kernel_ramdisk_second_info;
unordered_map<string,pair<unsigned,unsigned>> unpack_targets;
ss_necessary_manipulations ss; string_view magic; number_of_pages pages(page_size);

void decode_os_version(){
    os_version = os_version_patch_level >> 11;
    os_patch_level = os_version_patch_level & ((1<<11) - 1);
    decoded_os_version = pair<array<unsigned,3>,array<unsigned,2>>
                         {get_os_version(os_version),get_os_patch_level(os_patch_level)};
}

namespace hdr {
    void o_base_size(){
        print::inf("* Kernel size: "+ to_string(kernel_size));
        print::inf("* RAMdisk size: "+to_string(ramdisk_size));
    }
    void o_a_s_base(){}
    void o_a_l_base(){}
    void e_base(){
        kernel_pages   = pages.get(kernel_size);
        kernel_offset  = page_size * 1;
        unpack_targets["kernel"] = pair<unsigned,unsigned>{kernel_offset,kernel_size};
        ramdisk_pages  = pages.get(ramdisk_size);
        ramdisk_offset = page_size * (1 + kernel_pages);
        unpack_targets["ramdisk"] = pair<unsigned,unsigned>{ramdisk_offset,ramdisk_size};
    }
    void o_os_ver(){
        print::inf("* OS version: "+    ((decoded_os_version.first[0]!=0 || decoded_os_version.first[1]!=0 || decoded_os_version.first[2]!=0)
                                         ? to_string(decoded_os_version.first[0])+'.'
                                           +to_string(decoded_os_version.first[1])+'.'
                                           +to_string(decoded_os_version.first[2])
                                         : ""));
        print::inf("* OS patch level: "+ss.os_pl(decoded_os_version.second[0],decoded_os_version.second[1]));
    }
    void o_hdr(){
        print::inf("* Header version: "+to_string(kernel_ramdisk_second_info[8]));
    }
    void o_cmdline(){
        print::inf("* Command line: "+cmdline);
    }
    void bohdr0(){
        o_base_size();
        ss.hexize(kernel_addr);
        print::inf("* Kernel load address: "+ ss.ss.str());
        ss.hexize(ramdisk_addr);
        print::inf("* RAMdisk load address: "+ss.ss.str());
        print::inf("* second size: "+         to_string(second_size));
        ss.hexize(second_addr);
        print::inf("* second load address: "+ ss.ss.str());
        ss.hexize(tags_addr);
        print::inf("* Tags load address: "+   ss.ss.str());
        ss.hexize(page_size); // unpack_bootimg shows page size as hexadecimal
        print::inf("* Page size: "+           to_string(page_size)+" ("+ss.ss.str()+")");
        o_os_ver();
        o_hdr();
        print::inf("* Product name: "+        name);
        o_cmdline();
        //print::inf("* SHA checksum: "+        sha);
        print::inf("* Additional cmdline: "+  extra_cmdline);
    }
    void brhdr0(){
        kernel_size  = kernel_ramdisk_second_info[0];
        kernel_addr  = kernel_ramdisk_second_info[1];
        ramdisk_size = kernel_ramdisk_second_info[2];
        ramdisk_addr = kernel_ramdisk_second_info[3];
        second_size  = kernel_ramdisk_second_info[4];
        second_addr  = kernel_ramdisk_second_info[5];
        tags_addr    = kernel_ramdisk_second_info[6];
        page_size    = kernel_ramdisk_second_info[7];
        image.read(reinterpret_cast<char*>(&os_version_patch_level),4);
        decode_os_version();
        image.read(name_data.data(),16);
        name = name_data.data();
        cmdline_data.resize(513);
        image.read(cmdline_data.data(),512);
        cmdline = cmdline_data.data();
        image.seekg(32,ios::cur); // ignore SHA;
        //image.read(sha_data.data(),32);
        //sha = sha_data.data();
        image.read(extra_cmdline_data.data(),1024);
        extra_cmdline = extra_cmdline_data.data();
    }
    void behdr0(){
        e_base();
        if (second_size > 0){
            second_offset = page_size * (1 + kernel_pages + ramdisk_pages);
            unpack_targets["second"] = pair<unsigned,unsigned>{second_offset,second_size};
        }
    }
    void bohdr1(){
        bohdr0();
        print::inf("* Recovery DTBO size: "+  to_string(recovery_dtbo_size));
        ss.hexize(recovery_dtbo_offset);
        print::inf("* Recovery DTBO offset: "+ss.ss.str());
        print::inf("* Header size: "+         to_string(header_size));
    }
    void brhdr1(){
        brhdr0();
        image.read(reinterpret_cast<char*>(&recovery_dtbo_size),4);
        image.read(reinterpret_cast<char*>(&recovery_dtbo_offset),8);
        //header_size = BOOT_IMAGE_HEADER_V1_SIZE;
        image.read(reinterpret_cast<char*>(&header_size),4);
    }
    void behdr1(){
        behdr0();
        if (recovery_dtbo_size > 0)
        unpack_targets["recovery_dtbo"] = pair<unsigned,unsigned>{recovery_dtbo_offset,recovery_dtbo_size};
    }
    void bohdr2(){
        bohdr1();
        print::inf("* Device Tree Blob size: "+to_string(dtb_size));
        ss.hexize(dtb_addr);
        print::inf("* DTB load address: "+          ss.ss.str());
    }
    void brhdr2(){
        brhdr1();
        image.read(reinterpret_cast<char*>(&dtb_size),4);
        image.read(reinterpret_cast<char*>(&dtb_addr),8);
    }
    void behdr2(){
        behdr1();
        second_pages = pages.get(second_size);
        recovery_dtbo_pages = pages.get(recovery_dtbo_size);
        dtb_offset = page_size * (1 + kernel_pages + ramdisk_pages + second_pages + recovery_dtbo_pages);
        unpack_targets["dtb"] = pair<unsigned,unsigned>{dtb_offset,dtb_size};
    }
    void bohdr3(){
        o_base_size();
        o_os_ver();
        o_hdr();
        o_cmdline();
    }
    void brhdr3(){
        kernel_size = kernel_ramdisk_second_info[0];
        ramdisk_size = kernel_ramdisk_second_info[1];
        os_version_patch_level = kernel_ramdisk_second_info[2];
        decode_os_version();
        page_size = BOOT_IMAGE_HEADER_V34_PAGESIZE;
        cmdline_data.resize(v34_BOOT_ARGS_SIZE);
        image.read(cmdline_data.data(),v34_BOOT_ARGS_SIZE);
        cmdline = cmdline_data.data();
    }
    void behdr3(){
        e_base();
    }
    using nortrnfnc = array<void(*)(),4>;
    constexpr nortrnfnc brhdrs = {hdr::brhdr0,hdr::brhdr1,hdr::brhdr2,hdr::brhdr3,};
    constexpr nortrnfnc bohdrs = {hdr::bohdr0,hdr::bohdr1,hdr::bohdr2,hdr::bohdr3,};
    //constexpr nortrnfnc boahdr = {hdr::boahdr0,hdr::boahdr1,hdr::boahdr2,hdr::boahdr3,};
    constexpr nortrnfnc behdrs = {hdr::behdr0,hdr::behdr1,hdr::behdr2,hdr::behdr3,};
    void bhdr(){
        image_data.resize(image_size);
        image.read(reinterpret_cast<char*>(kernel_ramdisk_second_info.data()),36);
        brhdrs[kernel_ramdisk_second_info[8]]();
        if (!parser.get<bool>("--quiet")){
            bohdrs[kernel_ramdisk_second_info[8]]();
            //boahdr[kernel_ramdisk_second_info[8]]();
        }
    }
}

int main(const int argc, const char **argv) {
    parser.add_description(GVATC_TOOL_NAME" - The lightweight and fast tool to unpack Android bootable images.");
    parser.add_epilog("Tool to unpack Android-specific 'boot' and 'vendor_boot' bootable images. Non-commercial use only!\n"
                      "The part of GoldenVadim's Android Tools Collection. https://goldenvadim.github.io/GVATC");

    parser.add_argument("-i","--image")
    .help("Path to Android bootable image.")
    .metavar("<ANDROID!/VNDRBOOT>")
    .required();
    parser.add_argument("-o","--output-dir")
    .help("Path to output directory of files used in image (kernel, ramdisk, dtb, bootconfig, recovery dtbo and second)")
    .metavar("<DIR>")
    .default_value("");
    parser.add_argument("-H","--header-only")
    .help("Only give header information and don't extract files from image.")
    .flag();
    parser.add_argument("-q","--quiet")
    .help("Don't write header information to stdout from image.")
    .flag();
    /*parser.add_argument("-c","--command-args")
    .help("Write gabootimg command arguments based on header information.")
    .flag();
    parser.add_argument("--long-args")
    .help("Use long gabootimg command arguments flags instead of short.")
    .flag();
    parser.add_argument("--mkbootimg")
    .help("Also write command arguments for mkbootimg.")
    .flag();*/

    try { 
        parser.parse_args(argc,argv);

        print::inf("Checking arguments...");
        image_path = parser.get<string>("--image");
        if (!exists(image_path)){
            print::err("Invalid bootable image path.");
            return 1;
        }

        if (!parser.get<bool>("--header-only")){
            directory_output_path = parser.get<string>("--output-dir");
            if (directory_output_path.empty()){
                print::err("Directory of output path must not be empty");
                return 1;
            }
        }

        print::inf("Reading image...");
        image_size = file_size(image_path);
        if (image_size == 0) {
            print::err("This file is empty.");
            return 1;
        }
        image.open(image_path,ios::binary);
        if (!image.is_open()) {
            print::err("Failed to open this file.");
            return 1;
        }

        image_data.resize(BOOT_MAGIC_SIZE);
        image.read(image_data.data(),BOOT_MAGIC_SIZE);
        magic = string_view(image_data.data(),BOOT_MAGIC_SIZE);
        if (magic==BOOT_MAGIC){
            hdr::bhdr();
            if (!directory_output_path.empty()){
                hdr::behdrs[kernel_ramdisk_second_info[8]]();
                create_directories(directory_output_path);
                for (const pair<const string,pair<unsigned,unsigned>> &target : unpack_targets){
                    path temp;
                    temp = directory_output_path;
                    temp /= target.first;
                    ofstream file(temp);
                    if (!file.is_open()){
                        print::err("Failed to extract "+target.first);
                        exit(1);
                    }
                    image.seekg(target.second.first); // *_offset
                    buffer.resize(target.second.second); // *_size
                    image.read(buffer.data(),target.second.second);
                    file.write(buffer.data(),target.second.second);
                    buffer.clear();
                    file.close();
                }
            }
        } /*else if (magic==VENDOR_BOOT_MAGIC){
            hdr::vbhdr(image,parser);
        }*/else {
            print::err("Invalid magic.");
            return 1;
        }
        image.close();
    }
    catch (const exception &err) {
        print::err(err.what());
        return 1;
    }

    return 0;
}