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

# Fetch QtKeychain for storing secrets (e.g. the Redmine API key) in the OS-native secure storage
# (Secret Service/KWallet on Linux, Credential Manager on Windows, Keychain on macOS) instead of
# plain text. Used by QRedmine::Utils, see src/Utils/securestorage.h.
include(FetchContent)

set(BUILD_WITH_QT5 OFF CACHE BOOL "" FORCE)
set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
set(BUILD_TEST_APPLICATION OFF CACHE BOOL "" FORCE)
set(BUILD_QTQUICK_DEMO OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  qtkeychain
  GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
  GIT_TAG 0.17.0)
FetchContent_MakeAvailable(qtkeychain)

add_subdirectory(src)
add_subdirectory(tests)
