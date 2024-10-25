cmake_minimum_required(VERSION 3.16)

project(
    QRedmineClientExample
    LANGUAGES CXX
    VERSION 0.0.1)

find_package(Qt6 REQUIRED Core Network)

qt_standard_project_setup()

qt_add_executable(QRedmineClientExample main.cpp)

target_link_libraries(QRedmineClientExample PRIVATE QRedmineClient Qt6::Core
                                                    Qt6::Network)
