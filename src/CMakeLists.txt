# qt_add_library(redmineclient redmineclient.h redmineclient.cpp
# 	timetracker.h timetracker.cpp)

# target_link_libraries(redmineclient PRIVATE Qt6::Core Qt6::Network Qt6::Gui Qt6::Quick)



qt_add_executable(RedmineTimeTracker
	main.cpp
)

qt_add_qml_module(RedmineTimeTracker
	URI drook207.timeTracker
	VERSION 1.0
	QML_FILES
		qml/Main.qml
		qml/TimeTrackerView.qml
		SOURCES
		redmineclient.h redmineclient.cpp
				timetracker.h timetracker.cpp
)

target_link_libraries(RedmineTimeTracker PRIVATE
Qt6::Core Qt6::Network Qt6::Gui Qt6::Quick Qt6::Qml


)

