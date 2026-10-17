import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief Collapsible time-tracking control bar for the mobile UI.
 *
 * Collapsed, it shows just play/pause and the elapsed time so it stays out of the way; expanded,
 * it reveals the issue/comment/activity fields and the submit button. Functionally equivalent to
 * the desktop TrackingBar, which instead shows everything at once in a single wide row - there
 * isn't room for that on a phone screen.
 */
Rectangle {
    id: docroot

    required property TimeTracker timeTracker
    required property RedmineClient client
    required property SubmitDialog submitDialog
    required property var activitiesModel

    //! Activity to submit with the tracked time; 0 lets Redmine use its default.
    readonly property int selectedActivityId: activityCombo.activityId
    //! Whether the issue/comment/activity fields are visible. Toggled by the chevron button.
    property bool expanded: false

    //! Height of every control in the bar (44 on touch platforms, see Metrics).
    readonly property int controlHeight: Metrics.controlHeight

    color: Material.dialogColor
    implicitHeight: layout.implicitHeight + layout.anchors.topMargin + layout.anchors.bottomMargin

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
        id: layout
        anchors {
            fill: parent
            leftMargin: 16
            rightMargin: 16
            topMargin: 10
            bottomMargin: 12
        }
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            RoundButton {
                Layout.preferredWidth: docroot.controlHeight
                Layout.preferredHeight: docroot.controlHeight
                topInset: 0
                bottomInset: 0
                leftInset: 0
                rightInset: 0
                icon.width: 16
                icon.height: 16
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
                Layout.preferredWidth: docroot.controlHeight
                Layout.preferredHeight: docroot.controlHeight
                topInset: 0
                bottomInset: 0
                leftInset: 0
                rightInset: 0
                icon.width: 16
                icon.height: 16
                icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/pause.svg"
                highlighted: docroot.timeTracker.isTracking && !docroot.timeTracker.isPaused
                enabled: docroot.timeTracker.isTracking && !docroot.timeTracker.isPaused
                onClicked: docroot.timeTracker.pauseTracking()
            }

            Label {
                Layout.fillWidth: true
                text: docroot.timeTracker.elapsedTime
                font.pixelSize: 16
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
                Layout.preferredWidth: docroot.controlHeight
                Layout.preferredHeight: docroot.controlHeight
                topInset: 0
                bottomInset: 0
                leftInset: 0
                rightInset: 0
                flat: true
                icon.width: 16
                icon.height: 16
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

                IssueSearchInputField {
                    id: issueField
                    Layout.fillWidth: true
                    Layout.preferredHeight: docroot.controlHeight
                    timeTracker: docroot.timeTracker

                    onAccepted: {
                        docroot.client.getIssue(issueField.value)
                        inputValidatorTimer.stop()
                    }

                    onTextChanged: inputValidatorTimer.restart()

                    ClearButton {
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        visible: issueField.value >= 0 && !docroot.timeTracker.isTracking
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
                    Layout.preferredHeight: docroot.controlHeight
                    rightPadding: commentClearButton.width + 12
                    font.pixelSize: 13

                    ClearButton {
                        id: commentClearButton
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        visible: commentsField.text.length > 0
                        onClicked: commentsField.clear()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                LabeledField {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBottom
                    label: qsTr("Activity")

                    ActivityComboBox {
                        id: activityCombo
                        Layout.fillWidth: true
                        Layout.preferredHeight: docroot.controlHeight
                        font.pixelSize: 13
                        activitiesModel: docroot.activitiesModel
                        projectId: issueField.projectId
                    }
                }

                RoundButton {
                    Layout.alignment: Qt.AlignBottom
                    Layout.preferredWidth: docroot.controlHeight
                    Layout.preferredHeight: docroot.controlHeight
                    topInset: 0
                    bottomInset: 0
                    leftInset: 0
                    rightInset: 0
                    icon.width: 16
                    icon.height: 16
                    icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/plus.svg"
                    highlighted: true
                    enabled: docroot.timeTracker.isTracking
                    onClicked: {
                        docroot.submitDialog.comment = commentsField.text
                        docroot.submitDialog.issueId = issueField.value
                        docroot.submitDialog.activityId = docroot.selectedActivityId
                        docroot.submitDialog.customFields = [] // this bar has no Special field
                        docroot.submitDialog.open()
                    }
                }
            }
        }
    }
}
