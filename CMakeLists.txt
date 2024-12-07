cmake_minimum_required(VERSION 3.16)
project(HitboxFix)

#set(CMAKE_CXX_STANDARD 14)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(cmake-version4git)

PROJECT_VERSION_FROM_GIT()
add_definitions(${PROJECT_VERSION4GIT_CFLAGS})


add_library(HitboxFix SHARED
        include/cssdk/public/interface.cpp
		    src/mem/handles.cpp
		    src/mem/handles.cpp
		    src/mem/patternscan.cpp
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
        
        
         "include/shared/shared.h")

        
target_include_directories(HitboxFix PRIVATE include)
target_include_directories(HitboxFix PRIVATE include/cssdk/common)
target_include_directories(HitboxFix PRIVATE include/cssdk/dlls)
target_include_directories(HitboxFix PRIVATE include/cssdk/dlls/API)
target_include_directories(HitboxFix PRIVATE include/cssdk/dlls/bot)
target_include_directories(HitboxFix PRIVATE include/cssdk/dlls/hostage)
target_include_directories(HitboxFix PRIVATE include/cssdk/engine)
target_include_directories(HitboxFix PRIVATE include/cssdk/game_shared)
target_include_directories(HitboxFix PRIVATE include/cssdk/game_shared/bot)
target_include_directories(HitboxFix PRIVATE include/cssdk/pm_shared)
target_include_directories(HitboxFix PRIVATE include/cssdk/public)
target_include_directories(HitboxFix PRIVATE include/cssdk/public/cl_dll)
target_include_directories(HitboxFix PRIVATE include/cssdk/public/HLTV)
target_include_directories(HitboxFix PRIVATE include/cssdk/public/steam)
target_include_directories(HitboxFix PRIVATE include/cssdk/public/tier0)
target_include_directories(HitboxFix PRIVATE include/cssdk/public/vgui)
target_include_directories(HitboxFix PRIVATE include/metamod)
target_include_directories(HitboxFix PRIVATE src)
target_include_directories(HitboxFix PRIVATE src/mem)
target_include_directories(HitboxFix PRIVATE dep/subhook)
target_include_directories(HitboxFix PRIVATE dep/lz4)
set(SUBHOOK_STATIC "" ON )
set(SUBHOOK_TESTS  "" OFF )


add_subdirectory(dep/subhook) 
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")#-fvisibility=hidden -s -ffunction-sections -fdata-sections -fpermissive -Wno-write-strings -Wno-int-to-pointer-cast -Wno-unused-result -Wno-multichar 
   
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set_target_properties(HitboxFix PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" # static linking
    )
   target_link_options(HitboxFix PRIVATE /EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8 /SECTION:.data,RW)
    target_compile_options(HitboxFix PRIVATE 
   /std:c++20
    /ZI
    #/arch:AVX2 
    /Zc:strictStrings-
    #/EHsc   
    #/GR- 
    )
set_target_properties(subhook PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" # static linking
)

set_target_properties(HitboxFix PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" # static linking
)

    
target_link_libraries(HitboxFix PRIVATE psapi.lib dbghelp.lib Ws2_32.lib)
else()
     set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -std=c++14 -Wno-reserved-user-defined-literal -Wno-format -Wno-writable-strings -fpermissive -D_vsnprintf=vsnprintf")
    set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3 -shared")
    set(CMAKE_CXX_FLAGS_RELEASE "-Os -shared -s ")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -shared -s")
    #set(CMAKE_CXX_STANDARD 14)
    #set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}  -static-libstdc++ -static-libgcc")
    set_target_properties(HitboxFix PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32" POSITION_INDEPENDENT_CODE ON)

endif()


target_link_libraries(HitboxFix PRIVATE subhook LZ4)

set_target_properties(HitboxFix PROPERTIES PREFIX "")

add_subdirectory(dep/lz4)
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

set_target_properties(LZ4 PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" # static linking
)


add_subdirectory(vis)

set_target_properties(HitboxFix PROPERTIES OUTPUT_NAME "hitbox_fix_mm")
else()
set_target_properties(HitboxFix PROPERTIES OUTPUT_NAME "hitbox_fix_mm_i386")
endif()


