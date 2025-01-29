# Root CMake file for QRedmineClient Author: Pascal Berndt
#
# ##############################################################################

cmake_minimum_required(VERSION 3.16)

project(
	QRedmine
	LANGUAGES CXX
	VERSION 0.0.2)

find_package(Qt6 REQUIRED Core)

qt_standard_project_setup()

if(${CMAKE_SOURCE_DIR} EQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "Insource builds are not allowed")
endif()

add_subdirectory(src)
include(CPack)

set(CPACK_PACKAGE_NAME "RedmineTimeTracker")

set(CPACK_PACKAGE_VENDOR "drook207")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
