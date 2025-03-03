cmake_minimum_required(VERSION 3.16)

project(
	RedmineClient
	LANGUAGES CXX
	VERSION 0.0.1)

qt_standard_project_setup()

find_package(Qt6 REQUIRED Core Network Quick)

if(QT_KNOWN_POLICY_QTP0001)
	qt_policy(SET QTP0001 NEW)
endif()

# qt_add_library(redmineclient STATIC)

qt_add_qml_module(
	redmineclient
	URI
	drook207.redmineClient
	VERSION
	1.0
	SOURCES
	redmineclient.cpp
	redmineclient.h
	OUTPUT_DIRECTORY
	${CMAKE_BINARY_DIR}/qt/qml/drook207/redmineClient)

if(ANDROID)
	if(QT_KNOWN_POLICY_QTP0002)
		qt_policy(SET QTP0002 NEW)
	endif()
	set_target_properties(
		redmineclient
		PROPERTIES QT_ANDROID_PACKAGE_SOURCE_DIR
				   "${CMAKE_CURRENT_SOURCE_DIR}/android"
				   QT_ANDROID_PACKAGE_NAME "drook207.redmineclient"
				   QT_ANDROID_TARGET_SDK_VERSION 35
				   QT_ANDROID_VERSION_CODE 20
				   QT_ANDROID_VERSION_NAME "2.0")

	qt_import_plugins(
		redmineclient
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

if(IOS)
	set(asset_catalog_path "ios/Assets.xcassets")
	target_sources(redmineclient PRIVATE "${asset_catalog_path}")
	set_source_files_properties(${asset_catalog_path}
								PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	set_target_properties(
		redmineclient
		PROPERTIES XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon)
endif()
target_include_directories(redmineclient INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(redmineclient PRIVATE Qt6::Core Qt6::Network utils)
target_compile_definitions(redmineclient PRIVATE QT_REDMINECLIENT_BUILD)
install(
	TARGETS redmineclient
	BUNDLE DESTINATION .
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
