cmake_minimum_required(VERSION 4.1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
project(GVATC)

add_compile_options(-Wall -Wextra -Werror -funsigned-char -fno-signed-char)

file(GLOB common libgvatc/common/*)
file(GLOB abootimg libgvatc/abootimg/*)
add_library(gvatc SHARED libgvatc/libgvatc.cpp ${common} ${abootimg})

# ----- TOOLS ----- #
file(GLOB cabootimg cabootimg/*)
add_executable(cabootimg ${cabootimg})
add_executable(uabootimg uabootimg/main.cpp)

target_link_libraries(cabootimg PRIVATE gvatc)
target_link_libraries(uabootimg PRIVATE gvatc)

# ----- INSTA ----- #
include(GNUInstallDirs)
install(TARGETS gvatc cabootimg uabootimg
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)