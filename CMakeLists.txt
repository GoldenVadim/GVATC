cmake_minimum_required(VERSION 4.1)
set(CMAKE_CXX_STANDARD 17)
project(GVATC)

add_compile_options(-Wall -Wextra -Werror)

add_library(gvatc SHARED libgvatc.cpp libgvatc_common.h)

# ----- TOOLS ----- #
file(GLOB gabootimg gabootimg/*)
add_executable(gabootimg ${gabootimg})

target_link_libraries(gabootimg
        PRIVATE gvatc
)

# ----- INSTA ----- #
include(GNUInstallDirs)
install(TARGETS gvatc gabootimg
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)