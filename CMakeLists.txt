# Root CMake file for QRedmineClient Author: Pascal Berndt
#
# ##############################################################################

cmake_minimum_required(VERSION 3.16)

project(
	Stride
	LANGUAGES CXX
	VERSION 0.2.0)

# Pre-release suffix of the version (SemVer, e.g. "beta.1" for 0.2.0-beta.1),
# empty for a final release. project(VERSION) only takes numbers, so the suffix
# lives here; STRIDE_VERSION is the full version the app shows and the release
# tag (v<STRIDE_VERSION>) must match, see .github/workflows/release.yml.
set(STRIDE_VERSION_SUFFIX "beta.1")
if(STRIDE_VERSION_SUFFIX)
	set(STRIDE_VERSION "${PROJECT_VERSION}-${STRIDE_VERSION_SUFFIX}")
else()
	set(STRIDE_VERSION "${PROJECT_VERSION}")
endif()

find_package(Qt6 REQUIRED Core)

qt_standard_project_setup()

# All QML modules go below one directory, so tools like qmllint and qmlls find
# each module's imports (e.g. Stride using QRedmine.Client) there.
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qt/qml)

if(${CMAKE_SOURCE_DIR} EQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "Insource builds are not allowed")
endif()

# Fetch QtKeychain for storing secrets (e.g. the Redmine API key) in the
# OS-native secure storage (Secret Service/KWallet on Linux, Credential Manager
# on Windows, Keychain on macOS) instead of plain text. Used by QRedmine::Utils,
# see src/Utils/securestorage.h.
include(FetchContent)

set(BUILD_WITH_QT5
	OFF
	CACHE BOOL "" FORCE)
set(BUILD_TRANSLATIONS
	OFF
	CACHE BOOL "" FORCE)
set(BUILD_TEST_APPLICATION
	OFF
	CACHE BOOL "" FORCE)
set(BUILD_QTQUICK_DEMO
	OFF
	CACHE BOOL "" FORCE)

FetchContent_Declare(
	qtkeychain
	GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
	GIT_TAG 0.17.0)

# Unlike FetchContent_MakeAvailable(), add the subdirectory with
# EXCLUDE_FROM_ALL so QtKeychain's own install() rules (its dev headers, CMake
# package config, translations) never register - CPack packages every install()
# rule in the whole build, and an end user of Stride has no use for QtKeychain's
# SDK files, only the shared library qt_generate_deploy_qml_app_script() already
# copies in as a runtime dependency.
FetchContent_GetProperties(qtkeychain)
if(NOT qtkeychain_POPULATED)
	FetchContent_Populate(qtkeychain)
	add_subdirectory(${qtkeychain_SOURCE_DIR} ${qtkeychain_BINARY_DIR}
					 EXCLUDE_FROM_ALL)
endif()

# Install everything into a single named component instead of CMake's
# "Unspecified" default, so the Qt Installer Framework package can attach its
# component script to it (see the CPack section of src/Stride/CMakeLists.txt).
# Set after QtKeychain, whose install rules are excluded.
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "Stride")

add_subdirectory(src)
add_subdirectory(tests)
