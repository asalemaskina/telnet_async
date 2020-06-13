cmake_minimum_required(VERSION 3.13)
project(telnet-async)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)
include_directories (/usr/local/include/event2)
link_directories (/usr/local/lib)
link_libraries (event)

add_executable(telnet-async
        main.cpp
        telnet.h)
