import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import drook207.timeTracker
import drook207.redmineClient

GroupBox {
    id: docroot
    title: "Entries from Id"

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
                Layout.preferredHeight: 80
                enabled: !docroot.timeTracker.isTracking
            }

            Button {
                text: "Get"
                onClicked: docroot.client.getTimeEntries(issueField.value)
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
                                text: "Issue #" + issueField.value
                                font.bold: true
                            }
                            Label {
                                text: modelData.hours.toFixed(2) + " hours"
                                Layout.leftMargin: 10
                            }
                            Label {
                                text: Qt.formatDateTime(modelData.spent_on,
                                                        "yyyy-MM-dd hh:mm")
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
