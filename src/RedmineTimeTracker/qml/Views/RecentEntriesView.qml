import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QRedmine.TimeTracker

GroupBox {
	id: docroot

	required property TimeTracker timeTracker

	title: qsTr("Recent Time Entries")
	Layout.fillWidth: true
	Layout.fillHeight: true

	ListView {
		id: recentEntries
		anchors.fill: parent
		model: docroot.timeTracker.recentEntries
		clip: true

		delegate: ItemDelegate {
			width: parent.width
			contentItem: ColumnLayout {
				spacing: 5

				RowLayout {
					Layout.fillWidth: true
					Label {
						text: qsTr("Id: %0").arg(modelData.id)
						font.bold: true
					}
					Label {
						text: qsTr("Issue #%0").arg(modelData.issue_id)
						font.bold: true
					}
					Label {
						text: qsTr("%0 hours").arg(modelData.hours.toFixed(2))
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
