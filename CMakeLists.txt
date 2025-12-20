cmake_minimum_required(VERSION 4.1)
set(CMAKE_CXX_STANDARD 17)
project(GVATC)

add_compile_options(-Wall -Wextra -Werror)

add_library(gvatc SHARED libgvatc.cpp libgvatc_common.h)

# ----- TOOLS ----- #
file(GLOB mabootimg mabootimg/*)
add_executable(mabootimg ${mabootimg})

target_link_libraries(mabootimg
        PRIVATE gvatc
)

# ----- INSTA ----- #
include(GNUInstallDirs)
install(TARGETS gvatc mabootimg
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)