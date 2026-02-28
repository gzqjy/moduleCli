cmake_minimum_required(VERSION 3.10)
project(moduleCli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

add_executable(moduleCli main.cpp)
target_link_libraries(moduleCli PRIVATE OpenSSL::Crypto)
