# cella SQL 编译器 —— CMake 构建脚本
cmake_minimum_required(VERSION 3.16)
project(cella_sql LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cella_sql
    src/main.cpp
    src/cella_lexer.cpp
    src/cella_parser.cpp
    src/cella_semantic.cpp
    src/cella_planner.cpp
    src/cella_optimizer.cpp
    src/cella_printer.cpp
)

target_include_directories(cella_sql PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    # 源码统一 UTF-8
    target_compile_options(cella_sql PRIVATE /utf-8 /W3)
else()
    target_compile_options(cella_sql PRIVATE -Wall -Wextra)
endif()
