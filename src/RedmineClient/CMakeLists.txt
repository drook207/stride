cmake_minimum_required(VERSION 3.16)

project(
	RedmineClient
	LANGUAGES CXX
	VERSION 0.0.1)

qt_standard_project_setup()

find_package(Qt6 REQUIRED Core Network)

qt_add_library(redmineclient SHARED redmineclient.cpp redmineclient.h)

target_link_libraries(redmineclient PRIVATE Qt6::Core Qt6::Network)

install(
	TARGETS redmineclient
	EXPORT redmineclientTargets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	INCLUDES
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
