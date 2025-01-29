import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import drook207.timeTracker
import drook207.redmineClient

GroupBox {
	id: docroot
	title: "Entries from Id"
	property TimeTracker timeTracker: undefined
	property RedmineClient client: undefined
	anchors.fill: parent
	ColumnLayout {
		anchors.fill: parent
		spacing: 10
		GridLayout {
			columns: 3
			Layout.fillWidth: true
			rowSpacing: 10
			columnSpacing: 10

			Label {
				text: "Issue ID:"
			}
			TextField {
				id: issueId
				Layout.fillWidth: true
				placeholderText: "Enter issue ID"
				validator: IntValidator {
					bottom: 1
				}
				enabled: !docroot.timeTracker.isTracking
			}

			Button {
				text: "Get"
				onClicked: docroot.client.getTimeEntries(issueId.text)
			}
		}
		GroupBox {
			title: "Time Entries"
			Layout.fillWidth: true
			Layout.fillHeight: true
			ListView {
				id: entriesList
				anchors.fill: parent
				model: docroot.timeTracker.entriesFromId
				clip: true

				delegate: ItemDelegate {
					width: parent.width
					contentItem: ColumnLayout {
						spacing: 5

						RowLayout {
							Layout.fillWidth: true
							Label {
								text: "Id: " + modelData.id
								font.bold: true
							}
							Label {
								text: "Issue #" + issueId.text
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
	}
}
