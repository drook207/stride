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
 Sql
 LinguistTools)

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

# Shared layout sizes, used as a QML singleton (see qml/Components/Metrics.qml).
set_source_files_properties(qml/Components/Metrics.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# Hours parsing/formatting for the time fields, a QML singleton too (see qml/Components/HoursFormat.qml).
set_source_files_properties(qml/Components/HoursFormat.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(
 QRedmineTimeTracker
 URI
  QRedmine.TimeTracker
 VERSION
 1.0
 DEPENDENCIES
 QRedmine.Client
 QML_FILES
 qml/Main.qml
 qml/Header.qml
 qml/Views/RecentEntriesView.qml
 qml/Views/GetTimeEntriesView.qml
 qml/Views/AddTimeEntryView.qml
 qml/Views/AddIssueView.qml
 qml/Views/SettingsView.qml
 qml/Components/SubmitDialog.qml
 qml/Components/MessageDialog.qml
 qml/Components/Sidebar.qml
 qml/Components/TrackingBar.qml
 qml/Components/AddDialog.qml
 qml/Components/LabeledField.qml
 qml/Components/DialogScrim.qml
 qml/Components/ClearButton.qml
 qml/Components/Metrics.qml
 qml/Components/HoursFormat.qml
 qml/Components/Card.qml
 qml/Components/PageTitle.qml
 qml/Components/FormPage.qml
 qml/Components/PrimaryButton.qml
 qml/Components/TimeEntryDelegate.qml
 qml/Components/DatePickerField.qml
 qml/Components/TimePickerField.qml
 qml/Components/TimeRangeRow.qml
 qml/Components/EditTimeEntryDialog.qml
 qml/Components/ActivityComboBox.qml
 qml/Components/SpecialFieldComboBox.qml
 qml/Desktop/DesktopShell.qml
 qml/Mobile/MobileShell.qml
 qml/Mobile/MobileHeader.qml
 qml/Mobile/MobileTrackingBar.qml
 qml/Mobile/MobileTabBar.qml
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
 qml/Assets/Icons/solid/clock-rotate-left.svg
 qml/Assets/Icons/solid/circle-plus.svg
 qml/Assets/Icons/solid/magnifying-glass.svg
 qml/Assets/Icons/solid/user.svg
 qml/Assets/Icons/solid/play.svg
 qml/Assets/Icons/solid/pause.svg
 qml/Assets/Icons/solid/xmark.svg
 qml/Assets/Icons/solid/chevron-down.svg
 qml/Assets/Icons/solid/chevron-up.svg
 qml/Assets/Icons/solid/chevron-left.svg
 qml/Assets/Icons/solid/chevron-right.svg
 qml/Assets/Icons/solid/calendar-days.svg
 qml/Assets/Icons/solid/clock.svg
 qml/Assets/Icons/solid/pen.svg
 qml/Assets/Icons/solid/plus.svg
 qml/Assets/Icons/app-icon.svg
 qml/Assets/Icons/app-icon.ico
 OUTPUT_DIRECTORY
 ${CMAKE_BINARY_DIR}/qt/qml/QRedmine/TimeTracker
  SOURCES issuemodel.h issuemodel.cpp
  QML_FILES
  SOURCES qmlexposedsortfiltermodelproxy.h qmlexposedsortfiltermodelproxy.cpp
  QML_FILES qml/Components/IssueSearchInputField.qml
  SOURCES issueinputvalidator.h issueinputvalidator.cpp
  SOURCES languagemanager.h languagemanager.cpp
 )

# Translation catalogs (see i18n/README.md). "update_translations" (not built by default) runs
# lupdate to refresh the .ts files from qsTr() call sites; every normal build runs lrelease to
# compile them to .qm and embeds them into the binary under the :/i18n/ resource prefix.
# MERGE_QT_TRANSLATIONS folds in Qt's own upstream catalogs (qtbase, qtdeclarative, ...) for each
# language, so Qt-provided strings (e.g. the Yes/No dialog buttons) are localized too.
qt_add_translations(
 QRedmineTimeTracker
 TS_FILES
 i18n/QRedmineTimeTracker_en.ts
 i18n/QRedmineTimeTracker_de.ts
 LUPDATE_OPTIONS
 -no-obsolete
 MERGE_QT_TRANSLATIONS)

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
# Application identity, used for QSettings and the data directory (see main.cpp). The version is
# the top-level project's.
target_compile_definitions(
 QRedmineTimeTracker
 PRIVATE APP_ORGANIZATION_NAME="QRedmine"
         APP_ORGANIZATION_DOMAIN="drook207.gitlab.io"
         APP_NAME="Stride"
         APP_VERSION="${CMAKE_PROJECT_VERSION}")

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

# Linux: the application icon, referenced by the desktop entry the IFW installer creates (see
# packaging/ifw/installscript.qs). Windows uses the icon embedded into the executable instead.
if(UNIX AND NOT APPLE)
  install(
   FILES qml/Assets/Icons/app-icon.svg
   DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
   RENAME qredminetimetracker.svg)
endif()

set(CPACK_PACKAGE_NAME "QRedmineTimeTracker")
set(CPACK_PACKAGE_VENDOR "drook207")
set(CPACK_PACKAGE_CONTACT "Pascal Berndt <berndt.pascal90@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Desktop time tracker for Redmine issues")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)

set(CPACK_PACKAGE_ICON
 "${CMAKE_SOURCE_DIR}/src/RedmineTimeTracker/qml/Assets/Icons/app-icon.ico")

# Only package the application component. CMake also knows QtKeychain's "Unspecified" component from
# its install rules, which are never run (EXCLUDE_FROM_ALL, see the root CMakeLists.txt), and would
# otherwise add an empty package for it to the installer.
set(CPACK_COMPONENTS_ALL "${CPACK_PACKAGE_NAME}")

# Windows and Linux: Qt Installer Framework (QtIFW) installer, see packaging/ifw/README.md.
# If binarycreator/repogen aren't found automatically, point CMake at your QtIFW install (the
# directory containing "bin", not "bin" itself) via the CPACK_IFW_ROOT or QTIFWDIR cache/environment
# variable, e.g. -DCPACK_IFW_ROOT=C:/Qt/Tools/QtInstallerFramework/4.11
set(IFW_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/packaging/ifw")

# CPackIFW only probes <Qt>/Tools/QtInstallerFramework/<major.minor.patch>, but the Qt Maintenance
# Tool installs recent versions as e.g. ".../4.11". Unless set explicitly, use the newest QtIFW found
# next to the Qt installation this project is built against (<Qt>/<version>/<arch>/../../Tools).
if(NOT CPACK_IFW_ROOT AND NOT DEFINED ENV{CPACK_IFW_ROOT} AND NOT DEFINED ENV{QTIFWDIR})
  file(GLOB _ifw_binarycreators
   "${QT6_INSTALL_PREFIX}/../../Tools/QtInstallerFramework/*/bin/binarycreator"
   "${QT6_INSTALL_PREFIX}/../../Tools/QtInstallerFramework/*/bin/binarycreator.exe")
  if(_ifw_binarycreators)
    list(SORT _ifw_binarycreators COMPARE NATURAL ORDER DESCENDING)
    list(GET _ifw_binarycreators 0 _ifw_binarycreator)
    cmake_path(GET _ifw_binarycreator PARENT_PATH _ifw_bin_dir)
    cmake_path(GET _ifw_bin_dir PARENT_PATH CPACK_IFW_ROOT)
    cmake_path(NORMAL_PATH CPACK_IFW_ROOT)
    message(STATUS "Using Qt Installer Framework from ${CPACK_IFW_ROOT}")
  endif()
endif()

set(CPACK_IFW_PACKAGE_TITLE "${CPACK_PACKAGE_NAME}")
set(CPACK_IFW_PACKAGE_PUBLISHER "${CPACK_PACKAGE_VENDOR}")
set(CPACK_IFW_PRODUCT_URL "https://gitlab.com/drook207/QRedmineTimeTracker")
set(CPACK_IFW_PACKAGE_START_MENU_DIRECTORY "${CPACK_PACKAGE_NAME}")

# Look & feel matching the application's Material Dark theme and accent color (qml/Main.qml).
# "Modern" is the only wizard style that renders identically on Windows and Linux and honors the
# banner/logo; the style sheet then recolors every widget of the wizard.
set(CPACK_IFW_PACKAGE_WIZARD_STYLE "Modern")
set(CPACK_IFW_PACKAGE_STYLE_SHEET "${IFW_RESOURCE_DIR}/style.qss")
set(CPACK_IFW_PACKAGE_RESOURCES "${IFW_RESOURCE_DIR}/resources.qrc")
set(CPACK_IFW_PACKAGE_TITLE_COLOR "#FFFFFF")
set(CPACK_IFW_PACKAGE_BANNER "${IFW_RESOURCE_DIR}/banner.png")
set(CPACK_IFW_PACKAGE_LOGO "${IFW_RESOURCE_DIR}/logo.png")
set(CPACK_IFW_PACKAGE_WINDOW_ICON "${IFW_RESOURCE_DIR}/window-icon.png")
set(CPACK_IFW_PACKAGE_WIZARD_SHOW_PAGE_LIST ON)
set(CPACK_IFW_PACKAGE_CONTROL_SCRIPT "${IFW_RESOURCE_DIR}/controlscript.qs")
set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_WIDTH 860)
set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_HEIGHT 560)

# Offer to start the application on the installer's last page.
set(CPACK_IFW_PACKAGE_RUN_PROGRAM "@TargetDir@/bin/QRedmineTimeTracker")
set(CPACK_IFW_PACKAGE_RUN_PROGRAM_DESCRIPTION "Start ${CPACK_PACKAGE_NAME}")

if(WIN32)
  set(CPACK_GENERATOR "IFW")
  set(CPACK_IFW_PACKAGE_ICON "${CPACK_PACKAGE_ICON}")
  # Program Files; the installer requests elevation for it automatically.
  set(CPACK_IFW_TARGET_DIRECTORY "@ApplicationsDir@/${CPACK_PACKAGE_NAME}")

elseif(APPLE)
  # macOS: a .dmg is the conventional way to hand out an app bundle.
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_DMG_VOLUME_NAME "${CPACK_PACKAGE_NAME}")

else()
  # Linux: the IFW installer by default. A portable .tar.gz (-G TGZ, no extra tooling required) or a
  # native package (-G DEB, needs dpkg-dev; -G RPM, needs rpm-build) can still be built by passing
  # the generator explicitly - the metadata below applies to those.
  set(CPACK_GENERATOR "IFW")
  # Per-user install that needs no root privileges, like the desktop entry the installer creates.
  set(CPACK_IFW_TARGET_DIRECTORY "@HomeDir@/${CPACK_PACKAGE_NAME}")

  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
  set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

  set(CPACK_RPM_PACKAGE_LICENSE "MIT")
  set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")
  set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
endif()

include(CPack)
include(CPackIFW)

# Every install() rule of the project lands in this one component (see
# CMAKE_INSTALL_DEFAULT_COMPONENT_NAME in the root CMakeLists.txt), which carries the IFW component
# script that creates the Start menu shortcut (Windows) / desktop entry (Linux).
cpack_add_component(
 ${CPACK_PACKAGE_NAME}
 DISPLAY_NAME "${CPACK_PACKAGE_NAME}"
 DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}"
 REQUIRED)
cpack_ifw_configure_component(
 ${CPACK_PACKAGE_NAME}
 FORCED_INSTALLATION
 SCRIPT "${IFW_RESOURCE_DIR}/installscript.qs"
 LICENSES "MIT License" "${CPACK_RESOURCE_FILE_LICENSE}")
