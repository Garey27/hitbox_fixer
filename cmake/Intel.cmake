#-------------------------------------------------------------------------------------------
# IPO options.
#-------------------------------------------------------------------------------------------

set(CMAKE_C_COMPILE_OPTIONS_IPO ${CMAKE_C_COMPILE_OPTIONS_IPO} -fno-fat-lto-objects)
set(CMAKE_CXX_COMPILE_OPTIONS_IPO ${CMAKE_CXX_COMPILE_OPTIONS_IPO} -fno-fat-lto-objects)


#-------------------------------------------------------------------------------------------
# Compile options.
#-------------------------------------------------------------------------------------------

# Diagnostic flags
option(ALL_DIAGNOSTIC_GROUPS "Enable all diagnostic groups." OFF)

target_compile_options(${PROJECT_NAME} PRIVATE
    -w3 -Wall -Wremarks -Wcheck -Weffc++
    -Wuninitialized -Wdeprecated -Wpointer-arith
    -diag-disable=383,1418,1419,1572,2012,2015

    # Build type Release, MinSizeRel, RelWithDebInfo
    $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>,$<CONFIG:RelWithDebInfo>>:
    -Werror -Werror-all -Wfatal-errors>
)

if(${ALL_DIAGNOSTIC_GROUPS})
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:
        -diag-enable=thread,power,port-win,openmp,warn,error,remark,vec,par,cpu-dispatch>)
endif()

# Debug flags
set(CMAKE_C_FLAGS_DEBUG "-O -g")
set(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})

# Release flags
set(CMAKE_C_FLAGS_RELEASE "-O3 -g0")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -g0")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")
set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
set(CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_MINSIZEREL})
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO})

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    -no-intel-extensions -m32 -mtune=generic -msse -msse2 -msse3 -mssse3 -mmmx
    -ffunction-sections -fdata-sections

    # Build type Release, MinSizeRel
    $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:
    -fno-stack-protector>
)

# Optional flags
if(${OPT_NO_RTTI})
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
endif()

if(${OPT_NO_EXCEPTIONS})
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
endif()


#-------------------------------------------------------------------------------------------
# Link options.
#-------------------------------------------------------------------------------------------

# Linker flags
target_link_options(${PROJECT_NAME} PRIVATE
    -no-intel-extensions -m32 -Wl,--fatal-warnings -Wl,--no-undefined -Wl,--as-needed -Wl,--gc-sections

    # Build type Release, MinSizeRel
    $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:
    -Wl,-O3 -Wl,--strip-all>
)

# Libraries linking
target_link_libraries(${PROJECT_NAME} PRIVATE -static-intel)

if(${OPT_LINK_LIBGCC})
    target_link_libraries(${PROJECT_NAME} PRIVATE -static-libgcc)
endif()

if(${OPT_LINK_LIBSTDC})
    target_link_libraries(${PROJECT_NAME} PRIVATE -static-libstdc++)
endif()

if(${OPT_LINK_C})
    target_link_libraries(${PROJECT_NAME} PRIVATE c)
endif()

if(${OPT_LINK_M})
    target_link_libraries(${PROJECT_NAME} PRIVATE m)
endif()

if(${OPT_LINK_DL})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()
