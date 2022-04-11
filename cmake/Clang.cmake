#-------------------------------------------------------------------------------------------
# Compile options.
#-------------------------------------------------------------------------------------------

# Diagnostic flags
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic -Weffc++ -Wunused -Wold-style-cast
    -Wcast-align -Wnull-dereference -Wredundant-decls -Wdouble-promotion
    -Wno-non-virtual-dtor

    # Build type Release, MinSizeRel, RelWithDebInfo
    $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>,$<CONFIG:RelWithDebInfo>>:
    -Werror -Wfatal-errors>
)

# Debug flags
set(CMAKE_C_FLAGS_DEBUG "-Og -ggdb")
set(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})

# Release flags
set(CMAKE_C_FLAGS_RELEASE "-O3 -g0")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -g0")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
set(CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_MINSIZEREL})
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO})

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    -m32 -mtune=generic -march=x86-64 -msse -msse2 -msse3 -mssse3 -mmmx
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
    -m32 -Wl,--fatal-warnings -Wl,--no-undefined -Wl,--as-needed -Wl,--gc-sections

    # Build type Release, MinSizeRel
    $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:
    -Wl,-O3 -Wl,--strip-all>
)

# Libraries linking
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
