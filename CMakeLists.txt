cmake_minimum_required(VERSION 3.10)
project(moduleCli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(moduleCli main.cpp)
target_include_directories(moduleCli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
