set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
    message(FATAL_ERROR "This toolchain can only be run on Linux (got ${CMAKE_HOST_SYSTEM_NAME})")
elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL x86_64)
    set(CMAKE_C_FLAGS_INIT -m32)
    set(CMAKE_CXX_FLAGS_INIT -m32)
elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(i386|i486|i586|i686)$")
    # No op
else()
    message(FATAL_ERROR "This toolchain can only be run on x86/x64 processors (got ${CMAKE_HOST_SYSTEM_PROCESSOR})")
endif()