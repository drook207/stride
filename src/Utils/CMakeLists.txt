cmake_minimum_required(VERSION 3.16)

project(Utils
	LANGUAGES CXX
	VERSION 0.0.1)

qt_standard_project_setup()

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

qt_add_library(utils STATIC
	Utils_global.h
	qsettingshelper.h)


target_include_directories(utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(utils PRIVATE Qt${QT_VERSION_MAJOR}::Core)

target_compile_definitions(utils PRIVATE UTILS_LIBRARY)
