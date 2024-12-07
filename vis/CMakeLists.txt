cmake_minimum_required(VERSION 3.20)

project(HitboxVis)
set(BUILD_SHARED_LIBS OFF)
function(default_target_properties TARGET)
    target_include_directories("${TARGET}" PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/dep/LZ4"
        "${CMAKE_SOURCE_DIR}/include/shared"
        "${CMAKE_SOURCE_DIR}/include/cssdk/public"
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/rehlds"
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/rehlds/rehlds/public"
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/rehlds/rehlds/pm_shared"
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/rehlds/rehlds/common"
        "${CMAKE_CURRENT_SOURCE_DIR}/dep/rehlds/rehlds/public/rehlds"
	    "${CMAKE_CURRENT_SOURCE_DIR}/dep/soxr/src"
    )
    set_target_properties("${TARGET}" PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" # static linking
    )

endfunction()

set(HITBOXFIX_SOURCES
    "src/utils/ntdll.h"
    "src/utils/memory.hpp"
    "src/utils/memory.cpp"
    "src/utils/CDynPatcher.cpp"
    "src/utils/CSectionData.cpp"
    "src/utils/win32exception.hpp"
    "src/utils/win32exception.cpp"
    "src/auto_offset/auto_offset.hpp"
    "src/auto_offset/auto_offset.cpp"
    "src/engine/engine.hpp"
    "src/engine/engine.cpp"
    "src/clientdll/clientdll.hpp"
    "src/clientdll/clientdll.cpp"
    "src/main.cpp"
    "${CMAKE_SOURCE_DIR}/include/cssdk/public/utlbuffer.cpp"
    "src/utils/rib.cpp")

add_library(HitboxVis SHARED ${HITBOXFIX_SOURCES})

default_target_properties(HitboxVis)

set_target_properties(HitboxVis PROPERTIES
  PREFIX "" 
  #SUFFIX ".asi" 
  OUTPUT_NAME "hitbox_vis"
)

target_compile_options(HitboxVis PRIVATE
    "$<$<CONFIG:Debug>:/ZI>"
    "/arch:AVX2"
    "/Zc:strictStrings-"
)
target_link_options(HitboxVis PRIVATE
    "/SAFESEH:NO"
)
target_link_libraries(HitboxVis PRIVATE LZ4	)
