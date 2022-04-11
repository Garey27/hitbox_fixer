set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

set(flags)

if(CMAKE_VERSION VERSION_LESS "3.12.4")
  message(FATAL_ERROR "Minimum CMake 3.12.4 is required (got ${CMAKE_VERSION})")
endif()

# TODO: Check that the standard library is compiled with correct options
# -march=i686 -mfp-math=387 -mno-sse -mno-sse2 -mno-sse3 -mno-sse4.1

# TODO: It would be really good if we could have our own compiled standard
# library with -fno-PIC -fno-gnu-unique options (and maybe even with -flto)

## COMPATIBILITY OPTIONS

if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
    message(FATAL_ERROR "This toolchain can only be run on Linux (got ${CMAKE_HOST_SYSTEM_NAME})")
elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL x86_64)
    list(APPEND flags -m32)
elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(i386|i486|i586|i686)$")
    # No op
else()
    message(FATAL_ERROR "This toolchain can only be run on x86/x64 processors (got ${CMAKE_HOST_SYSTEM_PROCESSOR})")
endif()

# Disable producing symbols with STB_GNU_UNIQUE type
# in order to keep SYSV OS ABI
# NOTE: We can just use brandelf in order to change OS ABI to SYSV
# since we are stripping all exported symbols except the needed ones
# but I think it is better to keep both of these mechanisms
list(APPEND flags -fno-gnu-unique)
add_link_options(
    -Wl,--no-gnu-unique
    -fuse-ld=gold # --no-gnu-unique is only presented in GOLD linker
)

add_link_options(-static-libgcc -static-libstdc++ -Wl,--exclude-libs,libstdc++)
add_link_options(-ldl -lm -lrt)
set(THREADS_PREFER_PTHREAD_FLAG ON)

## OPTIMIZATION OPTIONS

# Disable PIC because it slows down the code on x86 platform
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
# We also need this because CMAKE_POSITION_INDEPENDENT_CODE does not disable
# -fPIC for linking
add_link_options(-fno-PIC)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# TODO: Should we disable it for the Debug build?
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
add_link_options(-flto=jobserver)

list(APPEND flags -fdata-sections -ffunction-sections)
add_link_options(-Wl,--gc-sections)


list(JOIN flags " " flags)
set(CMAKE_C_FLAGS_INIT ${flags})
set(CMAKE_CXX_FLAGS_INIT ${flags})