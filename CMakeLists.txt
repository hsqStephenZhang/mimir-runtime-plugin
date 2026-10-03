cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(runtime)

if(NOT COMMAND add_mim_plugin)
    find_package(mim REQUIRED)
endif()

add_mim_plugin(runtime
    SOURCES
        src/runtime.cpp
        src/normalizers.cpp
        src/phase/ll_emit.cpp
    INSTALL
)
