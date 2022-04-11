#-------------------------------------------------------------------------------------------
# Enable Clang-Tidy checks.
#-------------------------------------------------------------------------------------------

set(CLANG_TIDY -checks=-*,
    boost-*,
    bugprone-*,
    clang-analyzer-*,
    cppcoreguidelines-*,
    misc-*,
    modernize-*,
    mpi-*,
    openmp-*,
    performance-*,
    portability-*,
    readability-*,
)


#-------------------------------------------------------------------------------------------
# Disable Clang-Tidy checks.
#-------------------------------------------------------------------------------------------

set(CLANG_TIDY_SUPPRESS
    -cppcoreguidelines-avoid-c-arrays*,
    -cppcoreguidelines-avoid-magic-numbers*,
    -cppcoreguidelines-pro-type-reinterpret-cast*,
    -cppcoreguidelines-pro-bounds-pointer-arithmetic*,
    -cppcoreguidelines-pro-type-vararg*,
    -cppcoreguidelines-pro-bounds-array-to-pointer-decay*,
    -cppcoreguidelines-pro-bounds-constant-array-index*,
    -cppcoreguidelines-owning-memory*,
    -modernize-avoid-c-arrays*,
    -modernize-use-trailing-return-type*,
    -readability-implicit-bool-conversion*,
    -readability-magic-numbers*,
    -readability-named-parameter*
)


#-------------------------------------------------------------------------------------------
# Find and run clang-tidy.
#-------------------------------------------------------------------------------------------

if(OPT_RUN_CLANG_TIDY)
    find_program(CLANG_TIDY_COMMAND NAMES clang-tidy)

    if(CLANG_TIDY_COMMAND)
        string(REGEX REPLACE "\n$" "" TIDY_OPTIONS ${CLANG_TIDY}${CLANG_TIDY_SUPPRESS})
        set(CMAKE_C_CLANG_TIDY clang-tidy ${TIDY_OPTIONS})
        set(CMAKE_CXX_CLANG_TIDY clang-tidy ${TIDY_OPTIONS})
    else()
        message(STATUS "WARNING: OPT_RUN_CLANG_TIDY is ON but clang-tidy is not found!")
        set(CMAKE_C_CLANG_TIDY "" CACHE STRING "" FORCE)
        set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
    endif()
endif()
