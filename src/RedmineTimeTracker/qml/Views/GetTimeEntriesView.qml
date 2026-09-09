import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

GroupBox {
    id: docroot
	title: qsTr("Entries from Id")

    required property TimeTracker timeTracker
    required property RedmineClient client

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        GridLayout {
            columns: 1
            rows: 3
            Layout.fillWidth: true
            rowSpacing: 10
            columnSpacing: 10

            IssueSearchInputField {
                id: issueField
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                enabled: !docroot.timeTracker.isTracking
            }

            Button {
				text: qsTr("Get")
                onClicked: docroot.client.getTimeEntries(issueField.value)
            }
        }
        GroupBox {
			title: qsTr("Time Entries")
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
								text: qsTr("Id: %0").arg(modelData.id)
                                font.bold: true
                            }
                            Label {
								text: qsTr("Issue #%0").arg(modelData.issue.id)
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
