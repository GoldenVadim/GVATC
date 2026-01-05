cmake_minimum_required(VERSION 4.1)
set(CMAKE_CXX_STANDARD 17)
project(GVATC)

add_compile_options(-Wall -Wextra -Werror)

file(GLOB common libgvatc/common/*)
file(GLOB abootimg libgvatc/abootimg/*)
add_library(gvatc SHARED libgvatc/libgvatc.cpp gvatc.hpp ${common} ${abootimg})

# ----- TOOLS ----- #
file(GLOB gabootimg gabootimg/*)
add_executable(gabootimg ${gabootimg})
add_executable(uabootimg uabootimg/main.cpp)

target_link_libraries(gabootimg PRIVATE gvatc)
target_link_libraries(uabootimg PRIVATE gvatc)

# ----- INSTA ----- #
include(GNUInstallDirs)
install(TARGETS gvatc gabootimg uabootimg
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)