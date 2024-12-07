cmake_minimum_required(VERSION 3.20)

project(LZ4 LANGUAGES C)
file(GLOB LZ4_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
     
add_library(LZ4 STATIC ${LZ4_SOURCES})