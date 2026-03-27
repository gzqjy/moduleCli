cmake_minimum_required(VERSION 3.10)
project(moduleCli LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    # 0x0601 代表 Windows 7 (这是 Boost 建议的最小兼容版本)
    # 如果你只打算支持 Windows 10 及以上，可以设为 0x0A00
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

if (WIN32)
    add_compile_options("/utf-8")
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    add_definitions(-DUNICODE -D_UNICODE)
endif ()


set(BOOST_ROOT $ENV{BOOST_ROOT})
set(Boost_COMPILER "-vc142")
if (WIN32)
    set(Boost_DIR "${BOOST_ROOT}/lib32-msvc-14.2/cmake/Boost-1.87.0")
else ()
    set(Boost_DIR "${BOOST_ROOT}/lib/cmake/Boost-1.87.0")
endif ()
set(Boost_NO_SYSTEM_PATHS ON)

cmake_policy(SET CMP0074 NEW)

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif ()
if (POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif ()

find_package(Boost 1.87.0 REQUIRED COMPONENTS
        filesystem
        system
        thread
        date_time
        program_options
        process
        json
)

find_package(Boost REQUIRED)

add_executable(moduleCli main.cpp)
target_include_directories(moduleCli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(moduleCli PRIVATE ${Boost_INCLUDE_DIRS})
# 3. 显式链接（这会告诉 CMake 去找具体的库）
target_link_libraries(moduleCli PRIVATE Boost::filesystem Boost::process)
