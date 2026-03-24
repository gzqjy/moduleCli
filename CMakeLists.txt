cmake_minimum_required(VERSION 3.10)
project(moduleCli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)

add_executable(moduleCli main.cpp)
target_include_directories(moduleCli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(moduleCli PRIVATE ${Boost_INCLUDE_DIRS})
