#pragma once
#define GVATC_TOOL_NAME "mabootimg"
#define GVATC_TOOL_VERSION "2025.12.13"

#include <fstream>
#include <cstring>
#include <string>
#include "../libgvatc_common.h"
#include "argparse/argparse.hpp"
#include "bootimg.h"

using std::exception,std::function,std::stoi,std::hex,std::string,
      std::array,std::vector,std::pair,std::find,std::memset,std::memcpy,std::invalid_argument,
      std::ifstream,std::ofstream,std::ios,std::filesystem::exists,std::filesystem::file_size,std::filesystem::path,
      argparse::ArgumentParser;

constexpr array<uint32_t,4> header_versions = {0,1,2,3,};
constexpr array<uint32_t,4> page_sizes = {2048,4096,8192,16384}; // default: 2048
inline string hdr_chck;

inline string       action,           name,                    cmdline,            extra_cmdline,       vendor_cmdline;
inline uint32_t     header_version,   page_size,               os_version;
inline path         boot_output_path, vendor_boot_output_path,
                    kernel_path,      ramdisk_path,            dtb_path,           vendor_ramdisk_path;
inline vector<char> kernel_data,      ramdisk_data,            dtb_data,           vendor_ramdisk_data, pad;
inline unsigned     kernel_addr,      ramdisk_addr,            dtb_addr,           tags_addr,           base_addr;
inline size_t       kernel_size,      ramdisk_size,            dtb_size,           vendor_ramdisk_size,
                    name_size,        cmdline_size,            extra_cmdline_size, vendor_cmdline_size, pad_size;
inline vector<uint32_t>   os_version_,  os_patch_level_;
constexpr unsigned v34_boot_cmdline_size = BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE;
inline pair<const char*,size_t> boot_img_hdr, vendor_boot_img_hdr;