cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

project(telnet-client LANGUAGES CXX)

add_executable(${PROJECT_NAME} main.cpp spawn.hpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

find_package(Boost 1.71.0 REQUIRED COMPONENTS chrono context system thread)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Boost::chrono Boost::context Boost::system Boost::thread)
