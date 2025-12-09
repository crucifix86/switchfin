cmake_minimum_required(VERSION 3.15)

project(libjbc C)

# Only compile core files - utils.c conflicts with system headers
add_library(jbc STATIC
    jailbreak.c
    kernelrw.c
)

target_include_directories(jbc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
