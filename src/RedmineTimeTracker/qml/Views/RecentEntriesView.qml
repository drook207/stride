import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import drook207.timeTracker

GroupBox {
	id: docroot
	title: "Recent Time Entries"
	Layout.fillWidth: true
	Layout.fillHeight: true
	property TimeTracker timeTracker: TimeTracker {}
	ListView {
		id: recentEntries
		Layout.fillWidth: true
		Layout.fillHeight: true
		model: docroot.timeTracker.recentEntries
		clip: true

		delegate: ItemDelegate {
			Layout.fillWidth: true
			contentItem: ColumnLayout {
				spacing: 5

				RowLayout {
					Layout.fillWidth: true
					Label {
						text: "Id: " + modelData.id
						font.bold: true
					}
					Label {
						text: "Issue #" + modelData.issue_id
						font.bold: true
					}
					Label {
						text: modelData.hours.toFixed(2) + " hours"
						Layout.leftMargin: 10
					}
					Label {
						text: Qt.formatDateTime(modelData.spent_on, "yyyy-MM-dd hh:mm")
						Layout.fillWidth: true
						horizontalAlignment: Text.AlignRight
					}
					Label {
						text: modelData.sync_status === 0 ? false : true
						Layout.fillWidth: true
						horizontalAlignment: Text.AlignRight
					}
				}

				Label {
					text: modelData.comments
					visible: modelData.comments !== ""
					wrapMode: Text.WordWrap
					Layout.fillWidth: true
				}
			}
		}

		ScrollBar.vertical: ScrollBar {}
	}
}
