import QtQuick
import QtQml
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

Item {
    id: docroot
    property TimeTracker timeTracker: undefined
    property RedmineClient client: undefined
    property AddDialog addDialog: AddDialog {
        client: docroot.client
        timeTracker: docroot.timeTracker
    }
    function saveInput() {
        docroot.addDialog.spentOn = spentOnField.text
        docroot.addDialog.issueId = issueField.value
        var german = Qt.locale("de_DE")
        const number = Number.fromLocaleString(german, timeSpentField.text)
        docroot.addDialog.hours = number
    }

    Timer {
        id: inputValidatorTimer
        interval: 1000
        onTriggered: {

            docroot.saveInput()
        }
    }

    ColumnLayout {
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: 15
        }

        spacing: 15

        Label {
            text: qsTr("Add Time Entry")
            font.bold: true
            font.pixelSize: 18
            color: Material.foreground
        }

        ColumnLayout {
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

                    onAccepted: {
						if (acceptableInput) {
                            docroot.addDialog.issueId = issueField.value
                        }
                    }

                    onTextChanged: {
                        inputValidatorTimer.restart()
                    }
                }
            }

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Time spent (eg. 1,0)")

                TextField {
                    id: timeSpentField
                    Layout.fillWidth: true
                    validator: DoubleValidator {
                        bottom: 0
                        decimals: 2
                        notation: DoubleValidator.StandardNotation
                    }
                    onAccepted: {
                        if (acceptableInput) {
                            var german = Qt.locale("de_DE")
							const number = Number.fromLocaleString(german, text)
                            docroot.addDialog.hours = number
                        }
                    }
                    onTextChanged: {
                        inputValidatorTimer.restart()
                    }
                }
            }

            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Date (yyyy-mm-dd)")

                TextField {
                    id: spentOnField
                    Layout.fillWidth: true
                    validator: RegularExpressionValidator {
                        regularExpression: /^\d{4}-(?:0[1-9]|1[0-2])-(?:0[1-9]|[12]\d|3[01])$/
                    }
                    onAcceptableInputChanged: {
                        docroot.addDialog.spentOn = spentOnField.text
                    }
                }
            }
        }

        LabeledField {
            Layout.fillWidth: true
            label: qsTr("Comments")

            TextArea {
                id: commentsField
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                wrapMode: TextEdit.Wrap
            }
        }

        RoundButton {
            text: qsTr("Add")
            highlighted: true
            onClicked: {
                docroot.addDialog.comment = commentsField.text
                docroot.addDialog.open()
            }
        }
    }
}
