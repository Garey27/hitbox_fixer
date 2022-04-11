cmake_minimum_required(VERSION 3.16)
project(HitboxFix)

#set(CMAKE_CXX_STANDARD 14)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(cmake-version4git)

PROJECT_VERSION_FROM_GIT()
add_definitions(${PROJECT_VERSION4GIT_CFLAGS})

include_directories(include)
include_directories(include/cssdk/common)
include_directories(include/cssdk/dlls)
include_directories(include/cssdk/dlls/API)
include_directories(include/cssdk/dlls/bot)
include_directories(include/cssdk/dlls/hostage)
include_directories(include/cssdk/engine)
include_directories(include/cssdk/game_shared)
include_directories(include/cssdk/game_shared/bot)
include_directories(include/cssdk/pm_shared)
include_directories(include/cssdk/public)
include_directories(include/cssdk/public/cl_dll)
include_directories(include/cssdk/public/HLTV)
include_directories(include/cssdk/public/steam)
include_directories(include/cssdk/public/tier0)
include_directories(include/cssdk/public/vgui)
include_directories(include/metamod)
include_directories(msvc)
include_directories(msvc/Debug)
include_directories(msvc/Release)
include_directories(src)
include_directories(src/mem)
include_directories(dep/subhook)

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
        
        
        )

        
set(SUBHOOK_STATIC "" ON )
set(SUBHOOK_TESTS  "" OFF )


add_subdirectory(dep/subhook) 
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")#-fvisibility=hidden -s -ffunction-sections -fdata-sections -fpermissive -Wno-write-strings -Wno-int-to-pointer-cast -Wno-unused-result -Wno-multichar 
   
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set( CMAKE_SHARED_LINKER_FLAGS "/EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8" )
    set_target_properties(HitboxFix PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" # static linking
    )
    target_compile_options(HitboxFix PRIVATE 
   /std:c++14
    #/ZI
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

    
target_link_libraries(HitboxFix PRIVATE psapi.lib dbghelp.lib)
else()
     set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -std=c++14 -Wno-reserved-user-defined-literal -Wno-format -Wno-writable-strings -fpermissive -D_vsnprintf=vsnprintf")
    set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3 -shared")
    set(CMAKE_CXX_FLAGS_RELEASE "-Os -shared -s ")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -shared -s")
    #set(CMAKE_CXX_STANDARD 14)
    #set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
    set_target_properties(HitboxFix PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32" POSITION_INDEPENDENT_CODE ON)

endif()


target_link_libraries(HitboxFix PRIVATE subhook)

set_target_properties(HitboxFix PROPERTIES PREFIX "")

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
set_target_properties(HitboxFix PROPERTIES OUTPUT_NAME "hitbox_fix_mm")
else()
set_target_properties(HitboxFix PROPERTIES OUTPUT_NAME "hitbox_fix_mm_i386")
endif()
