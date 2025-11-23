cmake_minimum_required(VERSION 4.1)
set(CMAKE_CXX_STANDARD 20)
project(GVATC)

add_compile_options(-Wall -Wextra) # -Werror in stable branch

file(GLOB mabootimg mabootimg/*)
add_executable(mabootimg ${mabootimg})

include(GNUInstallDirs)
install(TARGETS mabootimg
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
