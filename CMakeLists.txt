cmake_minimum_required(VERSION 4.1)
set(CMAKE_CXX_STANDARD 20)
project(GVATC)

add_compile_options(-Wall -Wextra -Werror)

add_executable(abootimgm abootimgm/bootimg.h abootimgm/main.cpp)
