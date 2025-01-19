# Root CMake file for QRedmineClient Author: Pascal Berndt
#
# ##############################################################################

cmake_minimum_required(VERSION 3.16)

project(
	QRedmineClient
	LANGUAGES CXX
	VERSION 0.0.1)

find_package(
	Qt6
	REQUIRED
	Core
	Qml
	Quick
	Network
	Gui)

qt_standard_project_setup()

if(${CMAKE_SOURCE_DIR} EQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "Insource builds are not allowed")
endif()

add_subdirectory(src)
