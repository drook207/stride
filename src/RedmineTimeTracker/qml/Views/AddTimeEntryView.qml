import QtQuick
import QtQml
import QtQuick.Controls
import QtQuick.Layouts
import drook207.timeTracker
import drook207.redmineClient

GroupBox {
    id: docroot
    title: qsTr("Add Time Entry")
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
        }

        spacing: 5
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

                onAccepted: {
					if (acceptableInput) {
                        docroot.addDialog.issueId = issueField.value
                    }
                }

                onTextChanged: {
                    inputValidatorTimer.restart()
                }
            }

            TextField {
                id: timeSpentField
                Layout.fillWidth: true
                placeholderText: qsTr("Enter time spent (eg. 1,0)")
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

            TextField {
                id: spentOnField
                Layout.fillWidth: true
                placeholderText: qsTr("Enter date (yyyy-mm-dd)")
                validator: RegularExpressionValidator {
                    regularExpression: /^\d{4}-(?:0[1-9]|1[0-2])-(?:0[1-9]|[12]\d|3[01])$/
                }
                onAcceptableInputChanged: {
                    docroot.addDialog.spentOn = spentOnField.text
                }
            }
        }
        TextArea {
            id: commentsField
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            placeholderText: qsTr("Enter comments for this time entry")
            wrapMode: TextEdit.Wrap
        }

        Button {
            text: qsTr("Add")
            onClicked: {
                docroot.addDialog.comment = commentsField.text
                docroot.addDialog.open()
            }
        }
    }
}
