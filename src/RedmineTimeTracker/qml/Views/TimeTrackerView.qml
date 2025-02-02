import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import drook207.timeTracker
import drook207.redmineClient

Item {

	id: docroot
	required property TimeTracker timeTracker
	required property RedmineClient client
	required property SubmitDialog submitDialog

	Connections {
		target: docroot.client
		function onTimeEntriesReceived(entries) {
			docroot.timeTracker.addEntriesFromId(entries)
		}
	}
	ColumnLayout {
		anchors.fill: parent
		spacing: 5
		TabBar {
			id: bar
			Layout.fillWidth: true
			Layout.fillHeight: true
			TabButton {
				text: qsTr("Track Time")
			}
			TabButton {
				text: qsTr("Add Time Entry")
			}

			TabButton {
				text: qsTr("Get Time Entries")
			}
		}
		StackLayout {
			Layout.fillHeight: true
			Layout.fillWidth: true
			currentIndex: bar.currentIndex
			ColumnLayout {
				Layout.fillHeight: true
				Layout.fillWidth: true
				spacing: 5

				TrackTimeView {
					id: trackTimeView
					timeTracker: docroot.timeTracker
					client: docroot.client
					submitDialog: docroot.submitDialog
				}
				RecentEntriesView {
					id: recentEntriesView
					timeTracker: docroot.timeTracker
				}
			}

			AddTimeEntryView {
				id: addTimeEntryView
				timeTracker: docroot.timeTracker
				client: docroot.client
			}

			GetTimeEntriesView {
				id: getRecentEntriesView
				timeTracker: docroot.timeTracker
				client: docroot.client
			}
		}
	}
}
