import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

Rectangle {
    id: docroot

    required property TimeTracker timeTracker
    required property RedmineClient client
    required property SubmitDialog submitDialog
    required property var activitiesModel

    property int selectedActivityId: 0
    property bool expanded: false

    color: Material.dialogColor
    implicitHeight: expanded ? 296 : 72

    Behavior on implicitHeight {
        NumberAnimation {
            duration: 150
        }
    }

    Timer {
        id: inputValidatorTimer
        interval: 500
        onTriggered: {
            if (issueField.acceptableInput) {
                docroot.client.getIssue(issueField.value)
            }
        }
    }

    ColumnLayout {
        anchors {
            fill: parent
            leftMargin: 16
            rightMargin: 16
            topMargin: 10
            bottomMargin: 10
        }
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            RoundButton {
                icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/play.svg"
                enabled: (issueField.knownIssue && issueField.acceptableInput && issueField.value >= 0 && !docroot.timeTracker.isTracking)
                         || (docroot.timeTracker.isTracking && docroot.timeTracker.isPaused)
                onClicked: {
                    if (!docroot.timeTracker.isTracking) {
                        docroot.timeTracker.startTracking(parseInt(issueField.value))
                    } else if (docroot.timeTracker.isPaused) {
                        docroot.timeTracker.resumeTracking()
                    }
                    docroot.client.getIssue(issueField.value)
                }
            }

            RoundButton {
                icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/pause.svg"
                highlighted: docroot.timeTracker.isTracking && !docroot.timeTracker.isPaused
                enabled: docroot.timeTracker.isTracking && !docroot.timeTracker.isPaused
                onClicked: docroot.timeTracker.pauseTracking()
            }

            Label {
                Layout.fillWidth: true
                text: docroot.timeTracker.elapsedTime
                font.pixelSize: 18
                font.bold: true
                color: Material.foreground
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                visible: issueField.value >= 0
                text: "#" + issueField.value
                color: Material.hintTextColor
                font.pixelSize: 13
            }

            RoundButton {
                flat: true
                icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/" + (docroot.expanded ? "chevron-down.svg" : "chevron-up.svg")
                onClicked: docroot.expanded = !docroot.expanded
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: docroot.expanded

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Issue")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    IssueSearchInputField {
                        id: issueField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        timeTracker: docroot.timeTracker

                        onAccepted: {
                            docroot.client.getIssue(issueField.value)
                            inputValidatorTimer.stop()
                        }

                        onTextChanged: inputValidatorTimer.restart()
                    }

                    ToolButton {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 44
                        icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/xmark.svg"
                        onClicked: issueField.value = -1
                    }
                }
            }

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Comment")

                TextField {
                    id: commentsField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    font.pixelSize: 13
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                LabeledField {
                    Layout.fillWidth: true
                    label: qsTr("Activity")

                    ComboBox {
                        id: activityCombo
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        font.pixelSize: 13
                        model: docroot.activitiesModel.timeEntryActivities
                        textRole: "name"
                        valueRole: "id"
                        onActivated: docroot.selectedActivityId = activityCombo.currentValue
                    }
                }

                RoundButton {
                    Layout.alignment: Qt.AlignBottom
                    icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/plus.svg"
                    highlighted: true
                    enabled: docroot.timeTracker.isTracking
                    onClicked: {
                        docroot.submitDialog.comment = commentsField.text
                        docroot.submitDialog.issueId = issueField.value
                        docroot.submitDialog.activityId = docroot.selectedActivityId
                        docroot.submitDialog.open()
                    }
                }
            }
        }
    }
}
