cmake_minimum_required(VERSION 4.1)
set(CMAKE_CXX_STANDARD 17)
project(GVATC)

add_compile_options(-Wall -Wextra -Werror)

add_library(gvatc SHARED libgvatc_common.cpp libgvatc_common_print.hpp libgvatc_common_other.hpp libgvatc_abootimg_os_ver.cpp libgvatc_abootimg_os_ver_set.hpp libgvatc_abootimg_os_ver_get.hpp)

# ----- TOOLS ----- #
add_executable(gabootimg gabootimg.cpp)
add_executable(uabootimg uabootimg.cpp)

target_link_libraries(gabootimg PRIVATE gvatc)
target_link_libraries(uabootimg PRIVATE gvatc)

# ----- INSTA ----- #
include(GNUInstallDirs)
install(TARGETS gvatc gabootimg
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)