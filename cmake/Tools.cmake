#-------------------------------------------------------------------------------------------
# Set output directory for compiled files
#-------------------------------------------------------------------------------------------

function(set_binary_output_directory dir)
    get_property(languages GLOBAL PROPERTY ENABLED_LANGUAGES)

    if("C" IN_LIST languages)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${dir}/${CMAKE_BUILD_TYPE}"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${dir}/${CMAKE_BUILD_TYPE}"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${dir}/${CMAKE_BUILD_TYPE}")
    elseif("CXX" IN_LIST languages)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${dir}/${CMAKE_BUILD_TYPE}"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${dir}/${CMAKE_BUILD_TYPE}"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${dir}/${CMAKE_BUILD_TYPE}")
    endif()
endfunction()


#-------------------------------------------------------------------------------------------
# Set a developer-chosen build type (Debug, Release, RelWithDebInfo, MinSizeRel)
#-------------------------------------------------------------------------------------------

function(set_build_type)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        if(OPT_DEBUG)
            message(STATUS "Setting build type to Debug.")
            set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
        else()
            message(STATUS "Setting build type to Release.")
            set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
        endif()
    endif()
endfunction()


#-------------------------------------------------------------------------------------------
# Set default build parallel jobs (multiprocessor compilation)
#-------------------------------------------------------------------------------------------

function(set_default_parallel_jobs)
    if("x${CMAKE_BUILD_PARALLEL_LEVEL}" STREQUAL "x")
        include(ProcessorCount)
        ProcessorCount(NCORES)
        message(STATUS "Defaulting to ${NCORES} parallel jobs.")
        set(CMAKE_BUILD_PARALLEL_LEVEL ${NCORES})
    endif()
endfunction()


#-------------------------------------------------------------------------------------------
# Find source files in given directory and subdirectories
#-------------------------------------------------------------------------------------------

macro(find_source_files _base_dir _extensions _return_list)
    foreach(src_ext ${_extensions})
        file(GLOB_RECURSE _found_list "${_base_dir}/*.${src_ext}")
        list(APPEND ${_return_list} ${_found_list})
    endforeach()
endmacro()
