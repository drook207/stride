# Prints the version from version.cmake as NUMBER=, SUFFIX= and VERSION= lines
# on stdout, for scripts such as the release workflow. Run it with "cmake -P
# cmake/print-version.cmake".

include("${CMAKE_CURRENT_LIST_DIR}/version.cmake")

foreach(_line "NUMBER=${STRIDE_VERSION_NUMBER}"
			  "SUFFIX=${STRIDE_VERSION_SUFFIX}" "VERSION=${STRIDE_VERSION}")
	execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${_line}")
endforeach()
