cmake_minimum_required(VERSION 3.16)
project(BankEase LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(bankease main.cpp)

# Compiler warnings
if(MSVC)
    target_compile_options(bankease PRIVATE /W4)
else()
    target_compile_options(bankease PRIVATE -Wall -Wextra -Wpedantic)
endif()
