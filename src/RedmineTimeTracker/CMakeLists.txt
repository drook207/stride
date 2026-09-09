cmake_minimum_required(VERSION 3.16)

project(
 QRedmineTimeTracker
 LANGUAGES CXX
 VERSION 0.0.1)

qt_standard_project_setup()

find_package(
 Qt6
 REQUIRED
 Core
 Qml
 Quick
 Gui
 Sql)

if(QT_KNOWN_POLICY_QTP0001)
  qt_policy(SET QTP0001 NEW)
endif()

if(QT_KNOWN_POLICY_QTP0004)
  qt_policy(SET QTP0004 NEW)
endif()

if(WIN32)
  enable_language(RC)
  set(APP_ICON_RESOURCE_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/qml/Assets/Icons/app.rc)
endif()

qt_add_executable(QRedmineTimeTracker main.cpp ${APP_ICON_RESOURCE_WINDOWS})

qt_add_qml_module(
 QRedmineTimeTracker
 URI
  QRedmine.TimeTracker
 VERSION
 1.0
 QML_FILES
 qml/Main.qml
 qml/Header.qml
 qml/Views/TimeTrackerView.qml
 qml/Views/RecentEntriesView.qml
 qml/Views/TrackTimeView.qml
 qml/Views/GetTimeEntriesView.qml
 qml/Views/AddTimeEntryView.qml
 qml/Views/SettingsView.qml
 qml/Components/SubmitDialog.qml
 qml/Components/MessageDialog.qml
 qml/Components/BurgerMenu.qml
 qml/Components/AddDialog.qml
 qml/Components/ConnectionStateLamp.qml
 SOURCES
 timetracker.h
 timetracker.cpp
 database.h
 database.cpp
 timeentryactivitiesmodel.h
 timeentryactivitiesmodel.cpp
 RESOURCES
 qml/Assets/Fonts/Font-Awesome-6-Free-Regular-400.otf
 qml/Assets/Icons/solid/gear.svg
 qml/Assets/Icons/solid/bars.svg
 qml/Assets/Icons/solid/ellipsis-vertical.svg
 qml/Assets/Icons/app-icon.svg
 qml/Assets/Icons/app-icon.ico
 OUTPUT_DIRECTORY
 ${CMAKE_BINARY_DIR}/qt/qml/QRedmine/TimeTracker
  SOURCES issuemodel.h issuemodel.cpp
  QML_FILES
  SOURCES qmlexposedsortfiltermodelproxy.h qmlexposedsortfiltermodelproxy.cpp
  QML_FILES qml/Components/IssueSearchInputField.qml
  SOURCES issueinputvalidator.h issueinputvalidator.cpp
 )

if(ANDROID)
  if(QT_KNOWN_POLICY_QTP0002)
    qt_policy(SET QTP0002 NEW)
  endif()
  set_target_properties(
 QRedmineTimeTracker
  PROPERTIES QT_ANDROID_PACKAGE_SOURCE_DIR
       "${CMAKE_CURRENT_SOURCE_DIR}/android"
       QT_ANDROID_PACKAGE_NAME "QRedmine.RedmineTimeTracker"
       QT_ANDROID_TARGET_SDK_VERSION 35
       QT_ANDROID_VERSION_CODE 20
       QT_ANDROID_VERSION_NAME "2.0")

  qt_import_plugins(
  QRedmineTimeTracker
  INCLUDE_BY_TYPE
  imageformats
  Qt::QSvgPlugin
  EXCLUDE_BY_TYPE
  qmltooling
  EXCLUDE_BY_TYPE
  iconengines
  EXCLUDE_BY_TYPE
  networkinformation
  EXCLUDE_BY_TYPE
  tls
  EXCLUDE_BY_TYPE
  platforminputcontexts)
endif()

set_target_properties(QRedmineTimeTracker PROPERTIES WIN32_EXECUTABLE TRUE
             MACOSX_BUNDLE TRUE)

if(IOS)
  set(asset_catalog_path "ios/Assets.xcassets")
  target_sources(QRedmineTimeTracker PRIVATE "${asset_catalog_path}")
  set_source_files_properties(${asset_catalog_path}
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(
  QRedmineTimeTracker
  PROPERTIES XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon)
endif()
target_include_directories(QRedmineTimeTracker
         INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
 QRedmineTimeTracker
 PRIVATE Qt6::Core
   Qt6::Network
   Qt6::Gui
   Qt6::Quick
   Qt6::Qml
   Qt6::Sql
QRedmine::Client
   QRedmine::Utils)
install(
 TARGETS QRedmineTimeTracker
 BUNDLE DESTINATION .
 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

qt_generate_deploy_qml_app_script(
 TARGET
 QRedmineTimeTracker
 OUTPUT_SCRIPT
 deploy_script
 MACOS_BUNDLE_POST_BUILD
 NO_UNSUPPORTED_PLATFORM_ERROR
 DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM)
install(SCRIPT ${deploy_script})

include(CPack)

set(CPACK_PACKAGE_NAME "QRedmineTimeTracker")

set(CPACK_PACKAGE_VENDOR "drook207")

set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)

set(CPACK_NSIS_MUI_ICON
 "${CMAKE_SOURCE_DIR}/src/RedmineTimeTracker/qml/Assets/Icons/app-icon.ico")
set(CPACK_NSIS_MUI_UNIICON
 "${CMAKE_SOURCE_DIR}/src/RedmineTimeTracker/qml/Assets/Icons/app-icon.ico")
set(CPACK_CREATE_DESKTOP_LINKS QRedmine-Time-Tracker)
set(CPACK_PACKAGE_EXECUTABLES "QRedmine-Time-Tracker" "QRedmineTimeTracker")
