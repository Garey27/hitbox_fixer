#-------------------------------------------------------------------------------------------
# Run Cppcheck.
#-------------------------------------------------------------------------------------------

if(${OPT_RUN_CPPCHECK})
	include(ProcessorCount)
	find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)

	ProcessorCount(NCORES)
	set(PLATFORM_TYPE "unspecified")

	if(WIN32)
		set(PLATFORM_TYPE "win32A")
	elseif(UNIX)
		set(PLATFORM_TYPE "unix32")
	endif()

    list(APPEND CMAKE_CXX_CPPCHECK
		"-j ${NCORES}"
		"--quiet"
		"--relative-paths"
		"--platform=${PLATFORM_TYPE}"
		"--std=c++17"
		"--force"
		"--inline-suppr"
		"--max-ctu-depth=20"
		"--enable=warning,style,performance,missingInclude"
		#"--bug-hunting"
		#"--inconclusive"
		"--suppressions-list=${CMAKE_MODULE_PATH}/CppcheckSuppressions.txt"
	)
	set(CMAKE_C_CPPCHECK ${CMAKE_CXX_CPPCHECK})
endif()
