import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

Item {
    id: docroot

    required property TimeTracker timeTracker
    required property RedmineClient client

    ColumnLayout {
        anchors {
            fill: parent
            margins: 15
        }
        spacing: 15

        Label {
			text: qsTr("Entries from Id")
            font.bold: true
            font.pixelSize: 18
            color: Material.foreground
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Issue")

                IssueSearchInputField {
                    id: issueField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    timeTracker: docroot.timeTracker
                    enabled: !docroot.timeTracker.isTracking
                }
            }

            RoundButton {
				text: qsTr("Get")
                highlighted: true
                onClicked: docroot.client.getTimeEntries(issueField.value)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 5

            Label {
				text: qsTr("Time Entries")
                font.bold: true
                font.pixelSize: 14
                color: Material.foreground
            }

            ListView {
                id: entriesList
                Layout.fillWidth: true
                Layout.fillHeight: true
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
