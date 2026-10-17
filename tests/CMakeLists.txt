cmake_minimum_required(VERSION 3.16)
project(QRedmineTesting)

# Enable testing for the project
enable_testing()

# Include FetchContent for downloading Google Test
include(FetchContent)

# Download and configure Google Test
FetchContent_Declare(
 googletest
 GIT_REPOSITORY https://github.com/google/googletest.git
 GIT_TAG v1.15.2  # Specify the version you want
)
# Configure Google Test build options
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Don't add GoogleTest/GoogleMock's own install() rules to the build - CPack packages
# every install() rule in the whole project, not just QRedmineTimeTracker's, so leaving
# this on bundles GTest's development headers/libraries into the shipped app package.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


add_executable(qredminetesting
redmineclienttest.h redmineclienttest.cpp
redmineclientintegrationtest.h redmineclientintegrationtest.cpp
projectaccesstest.cpp
databasetest.cpp
timetrackertest.cpp
timeoverviewtest.cpp
reminderstest.cpp
timeentryexporttest.cpp
mockhttpserver.h mockhttpserver.cpp
testutils.h
main.cpp)

find_package(Qt6 REQUIRED Core Network Quick Sql)

# The app's storage and tracking logic, tested directly: its sources are built into the test binary
# (they belong to the app's QML module, which a test can't link).
set(APP_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/RedmineTimeTracker)
target_sources(qredminetesting PRIVATE ${APP_SOURCE_DIR}/database.h ${APP_SOURCE_DIR}/database.cpp
                                       ${APP_SOURCE_DIR}/timetracker.h ${APP_SOURCE_DIR}/timetracker.cpp
                                       ${APP_SOURCE_DIR}/timeoverview.h ${APP_SOURCE_DIR}/timeoverview.cpp
                                       ${APP_SOURCE_DIR}/reminders.h ${APP_SOURCE_DIR}/reminders.cpp
                                       ${APP_SOURCE_DIR}/timeentryexport.h ${APP_SOURCE_DIR}/timeentryexport.cpp)
target_include_directories(qredminetesting PRIVATE ${APP_SOURCE_DIR})

target_link_libraries(qredminetesting
 PRIVATE
QRedmine::Client
QRedmine::Utils
 GTest::gtest_main
 Qt6::Core
 Qt6::Network
 Qt6::Quick
 Qt6::Sql
)

include(GoogleTest)

# Automatically discover tests
# gtest_discover_tests(qredminetesting
# 	# Optional: Set a discovery timeout if you have many tests
# 	DISCOVERY_TIMEOUT 60
# 	# Optional: Add test properties
# 	PROPERTIES
# 		TIMEOUT 10  # Timeout in seconds for each test
# )


# You can also add a custom test that runs with specific options
# add_test(NAME mylib_tests_with_filter
#          COMMAND mylib_tests --gtest_filter=CalculatorTest.AdditionWorks)
# set_tests_properties(mylib_tests_with_filter PROPERTIES LABELS "smoke")
