# The version of Stride, in one place: the root CMakeLists.txt includes this
# file before project(), and the release workflow reads it with
# print-version.cmake, so the release tag is checked against exactly what CMake
# builds.
#
# STRIDE_VERSION_NUMBER is the numeric version project(VERSION) takes.
# STRIDE_VERSION_SUFFIX is the SemVer pre-release suffix ("beta.1" for
# 0.2.0-beta.1), empty for a final release. STRIDE_VERSION is the full version
# the app shows; the release tag must be v<STRIDE_VERSION>.

set(STRIDE_VERSION_NUMBER 0.2.0)
set(STRIDE_VERSION_SUFFIX "beta.1")

if(STRIDE_VERSION_SUFFIX)
	set(STRIDE_VERSION "${STRIDE_VERSION_NUMBER}-${STRIDE_VERSION_SUFFIX}")
else()
	set(STRIDE_VERSION "${STRIDE_VERSION_NUMBER}")
endif()
