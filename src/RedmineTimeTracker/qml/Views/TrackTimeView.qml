import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels
import QRedmine.Client

GroupBox {
    id: docroot
	title: qsTr("Time Tracking")
    Layout.fillWidth: true
    required property TimeTracker timeTracker
    required property RedmineClient client
    required property SubmitDialog submitDialog

    Timer {
        id: inputValidatorTimer
        interval: 500
        onTriggered: {
            if (issueField.acceptableInput) {
                client.getIssue(issueField.value)
            }
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
                Layout.preferredHeight: 60

                onAccepted: {
                    client.getIssue(issueField.value)
                    inputValidatorTimer.stop()
                }

                onTextChanged: {
                    inputValidatorTimer.restart()
                }
            }
            Label {
				text: qsTr("Elapsed Time:")
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
			placeholderText: qsTr("Enter comments for this time entry")
            wrapMode: TextEdit.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
				text: !docroot.timeTracker.isTracking ? qsTr(
															"Start Tracking") : docroot.timeTracker.isPaused ? qsTr("Resume") : qsTr(
																												   "Pause")
				enabled: (issueField.knownIssue && issueField.acceptableInput && issueField.value >= 0)
						 || docroot.timeTracker.isTracking
				highlighted: docroot.timeTracker.isTracking && !docroot.timeTracker.isPaused
                onClicked: {
                    if (!docroot.timeTracker.isTracking) {
						docroot.timeTracker.startTracking(parseInt(issueField.value))
                    } else if (docroot.timeTracker.isPaused) {
                        docroot.timeTracker.resumeTracking()
                    } else {
                        docroot.timeTracker.pauseTracking()
                    }
                    client.getIssue(issueField.value)
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
