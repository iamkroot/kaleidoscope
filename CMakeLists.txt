cmake_minimum_required(VERSION 3.20)
project(Kaleidoscope)

set(CMAKE_CXX_STANDARD 20)

find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(kaleidoscope main.cpp lexer.hpp ast.hpp parser.hpp codegen.cpp)

llvm_map_components_to_libnames(llvm_libs support core irreader passes)

target_link_libraries(kaleidoscope ${llvm_libs})
