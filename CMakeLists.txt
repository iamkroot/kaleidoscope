cmake_minimum_required(VERSION 3.20)
project(Kaleidoscope)

set(CMAKE_CXX_STANDARD 20)

find_package(LLVM 13.0.0 PATHS ~/llvm NO_DEFAULT_PATH REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs analysis executionengine support core instcombine object irreader passes orcjit runtimedyld native)

add_executable(kaleidoscope main.cpp lexer.hpp ast.hpp parser.hpp codegen.cpp KaleidoscopeJIT.h)
target_link_libraries(kaleidoscope ${llvm_libs})

add_executable(experiments experiments.cpp)
target_link_libraries(experiments ${llvm_libs})
