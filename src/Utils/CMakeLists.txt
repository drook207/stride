cmake_minimum_required(VERSION 3.16)

project(
	QRedmineUtils
	LANGUAGES CXX
	VERSION 0.0.1)

qt_standard_project_setup()

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

qt_add_library(
	qredmineutils
	SHARED
	Utils_global.h
	appdatapath.h
	qsettingshelper.h
	stringhelper.h
	logging.cpp
	logging.h
	logmanager.cpp
	logmanager.h
	securestorage.cpp
	securestorage.h)
add_library(QRedmine::Utils ALIAS qredmineutils)

target_include_directories(qredmineutils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qredmineutils PRIVATE Qt${QT_VERSION_MAJOR}::Core
											qt6keychain)

target_compile_definitions(qredmineutils PRIVATE UTILS_LIBRARY)
