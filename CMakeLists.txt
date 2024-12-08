cmake_minimum_required(VERSION 3.18)
project(HitboxFix VERSION 1.0 LANGUAGES CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(cmake-version4git)

PROJECT_VERSION_FROM_GIT()
add_compile_definitions(${PROJECT_VERSION4GIT_CFLAGS})

add_library(HitboxFix SHARED
    include/cssdk/public/interface.cpp
    src/dllapi.cpp
    src/engine_api.cpp
    src/engine_rehlds_api.cpp
    src/h_export.cpp
    src/main.cpp
    src/meta_api.cpp
    src/mod_regamedll_api.cpp
    src/sdk_util.cpp 
    src/animation.cpp
    include/cssdk/public/utlbuffer.cpp
    "${CMAKE_SOURCE_DIR}/utils/CDynPatcher.cpp"
    "${CMAKE_SOURCE_DIR}/utils/CSectionData.cpp"
    "include/shared/shared.h"
)

target_include_directories(HitboxFix PRIVATE 
    include
    include/cssdk/common
    include/cssdk/dlls
    include/cssdk/dlls/API
    include/cssdk/dlls/bot
    include/cssdk/dlls/hostage
    include/cssdk/engine
    include/cssdk/game_shared
    include/cssdk/game_shared/bot
    include/cssdk/pm_shared
    include/cssdk/public
    include/cssdk/public/cl_dll
    include/cssdk/public/HLTV
    include/cssdk/public/steam
    include/cssdk/public/tier0
    include/cssdk/public/vgui
    include/metamod
    src
    "${CMAKE_SOURCE_DIR}/utils"
    dep/subhook
    dep/lz4
)

set(SUBHOOK_STATIC ON)
set(SUBHOOK_TESTS OFF)

add_subdirectory(dep/subhook) 
add_subdirectory(dep/lz4)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_definitions(-D_CRT_SECURE_NO_WARNINGS)
    target_link_options(HitboxFix PRIVATE /EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8 /SECTION:.data,RW)
    target_compile_options(HitboxFix PRIVATE 
        /std:c++20
        /ZI
        /Zc:strictStrings-
    )
    set_target_properties(subhook PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    set_target_properties(HitboxFix PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_link_libraries(HitboxFix PRIVATE psapi.lib dbghelp.lib Ws2_32.lib)
else()
    target_compile_options(HitboxFix PRIVATE 
        -std=c++20
        -Wno-reserved-user-defined-literal
        -Wno-format
        -Wno-writable-strings
        -fpermissive
        -D_vsnprintf=vsnprintf
        -m32
    )
    target_link_options(HitboxFix PRIVATE -m32)
    target_compile_features(HitboxFix PUBLIC cxx_std_20)
    set_target_properties(HitboxFix PROPERTIES 
        COMPILE_FLAGS "-m32"
        LINK_FLAGS "-m32"
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(LZ4 PRIVATE -m32)
    target_link_options(LZ4 PRIVATE -m32)
    target_compile_options(subhook PRIVATE -m32)
    target_link_options(subhook PRIVATE -m32)
endif()

target_link_libraries(HitboxFix PRIVATE subhook LZ4)
set_target_properties(HitboxFix PROPERTIES PREFIX "")

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set_target_properties(LZ4 PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    add_subdirectory(vis)
    set_target_properties(HitboxFix PROPERTIES OUTPUT_NAME "hitbox_fix_mm")
else()
    set_target_properties(HitboxFix PROPERTIES OUTPUT_NAME "hitbox_fix_mm_i386")
endif()
