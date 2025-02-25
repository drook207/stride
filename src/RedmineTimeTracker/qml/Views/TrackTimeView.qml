import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels
import drook207.timeTracker
import drook207.redmineClient

GroupBox {
    id: docroot
    title: "Time Tracking"
    Layout.fillWidth: true
    required property TimeTracker timeTracker
    required property RedmineClient client
    required property SubmitDialog submitDialog

    Item {

        QmlExposedSortFilterModelProxy {
            id: proxyModel2
            filterRole: 258 // IdAndSubjectRole
            sourceModel: issueModel
            filterCaseSensitivity: Qt.CaseInsensitive
        }
    }

    ColumnLayout {
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }

        spacing: 10

        GridLayout {
            columns: 1
            rows: 4
            Layout.fillWidth: true
            rowSpacing: 10
            columnSpacing: 10

            IssueSearchInputField {
                id: issueField
                Layout.fillWidth: true
                Layout.preferredHeight: 120

                proxyModel: proxyModel2
            }

            // TextField {
            //     id: issueIdField
            //     Layout.fillWidth: true
            //     placeholderText: "Enter issue ID"
            //     validator: IntValidator {
            //         bottom: 1
            //     }
            //     enabled: !docroot.timeTracker.isTracking
            // }
            Label {
                text: "Elapsed Time:"
            }
            Label {
                text: docroot.timeTracker.elapsedTime
                font.pixelSize: 24
                font.bold: true
                color: docroot.timeTracker.isPaused ? "#FFA500" : "#008000"
            }
        }

        TextArea {
            id: commentsField
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            placeholderText: "Enter comments for this time entry"
            wrapMode: TextEdit.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: !docroot.timeTracker.isTracking ? "Start Tracking" : docroot.timeTracker.isPaused ? "Resume" : "Pause"
                enabled: issueField.value >= 0 || docroot.timeTracker.isTracking
                highlighted: docroot.timeTracker.isTracking
                             && !docroot.timeTracker.isPaused
                onClicked: {
                    if (!docroot.timeTracker.isTracking) {
                        docroot.timeTracker.startTracking(parseInt(
                                                              issueField.value))
                    } else if (docroot.timeTracker.isPaused) {
                        docroot.timeTracker.resumeTracking()
                    } else {
                        docroot.timeTracker.pauseTracking()
                    }
                }
            }

            Button {
                text: "Stop"
                enabled: docroot.timeTracker.isTracking
                onClicked: {
                    docroot.submitDialog.comment = commentsField.text
                    docroot.submitDialog.issueId = issueField.value
                    docroot.submitDialog.open()
                }
            }
        }
    }
}
