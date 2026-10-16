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

# Unlike FetchContent_MakeAvailable(), add the subdirectory with EXCLUDE_FROM_ALL so
# QtKeychain's own install() rules (its dev headers, CMake package config, translations)
# never register - CPack packages every install() rule in the whole build, and an end
# user of QRedmineTimeTracker has no use for QtKeychain's SDK files, only the shared
# library qt_generate_deploy_qml_app_script() already copies in as a runtime dependency.
FetchContent_GetProperties(qtkeychain)
if(NOT qtkeychain_POPULATED)
  FetchContent_Populate(qtkeychain)
  add_subdirectory(${qtkeychain_SOURCE_DIR} ${qtkeychain_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Install everything into a single named component instead of CMake's "Unspecified" default, so the
# Qt Installer Framework package can attach its component script to it (see the CPack section of
# src/RedmineTimeTracker/CMakeLists.txt). Set after QtKeychain, whose install rules are excluded.
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "QRedmineTimeTracker")

add_subdirectory(src)
add_subdirectory(tests)
