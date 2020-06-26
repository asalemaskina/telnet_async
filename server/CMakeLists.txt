cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
project(telnet-server)

set(CMAKE_CXX_STANDARD 20)

include_directories(.)
include_directories (/usr/local/include/event2)
link_directories (/usr/local/lib)
link_libraries (event)

add_executable(telnet-server
        main.cpp
        telnet.hpp)
