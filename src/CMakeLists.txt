qt_add_executable(RedmineTimeTracker main.cpp)

qt_add_qml_module(
	RedmineTimeTracker
	URI
	drook207.timeTracker
	VERSION
	1.0
	QML_FILES
	qml/Main.qml
	qml/TimeTrackerView.qml
	SOURCES
	redmineclient.h
	redmineclient.cpp
	timetracker.h
	timetracker.cpp
	SOURCES
	timetrackerdatabase.h
	timetrackerdatabase.cpp)

if(ANDROID)
	set_target_properties(
		RedmineTimeTracker
		PROPERTIES QT_ANDROID_PACKAGE_SOURCE_DIR
				   "${CMAKE_CURRENT_SOURCE_DIR}/android"
				   QT_ANDROID_PACKAGE_NAME "drook207.redminetimetracker"
				   QT_ANDROID_TARGET_SDK_VERSION 35
				   QT_ANDROID_VERSION_CODE 20
				   QT_ANDROID_VERSION_NAME "2.0")

	qt_import_plugins(
		RedmineTimeTracker
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

set_target_properties(RedmineTimeTracker PROPERTIES WIN32_EXECUTABLE TRUE
													MACOSX_BUNDLE TRUE)

if(IOS)
	set(asset_catalog_path "ios/Assets.xcassets")
	target_sources(RedmineTimeTracker PRIVATE "${asset_catalog_path}")
	set_source_files_properties(${asset_catalog_path}
								PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	set_target_properties(
		RedmineTimeTracker
		PROPERTIES XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon)
endif()

target_link_libraries(
	RedmineTimeTracker PRIVATE Qt6::Core Qt6::Network Qt6::Gui Qt6::Quick
							   Qt6::Qml Qt6::Sql)
install(
	TARGETS RedmineTimeTracker
	BUNDLE DESTINATION .
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

qt_generate_deploy_qml_app_script(
	TARGET
	RedmineTimeTracker
	OUTPUT_SCRIPT
	deploy_script
	MACOS_BUNDLE_POST_BUILD
	NO_UNSUPPORTED_PLATFORM_ERROR
	DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM)
install(SCRIPT ${deploy_script})
