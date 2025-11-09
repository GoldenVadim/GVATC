cmake_minimum_required(VERSION 4.1)
set(CMAKE_CXX_STANDARD 20)
project(GVATC)

add_compile_options(-Wall -Wextra) # -Werror in stable branch

include(GNUInstallDirs)

file(GLOB abootimgm abootimgm/*)
add_executable(abootimgm ${abootimgm})

install(TARGETS abootimgm
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)