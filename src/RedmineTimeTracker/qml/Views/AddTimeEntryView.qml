import QtQuick
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
	anchors.fill: parent
	ColumnLayout {
		anchors.fill: parent
		spacing: 5
		GridLayout {
			columns: 1
			rows: 3
			Layout.fillWidth: true
			rowSpacing: 10
			columnSpacing: 10
			TextField {
				id: issueIdField
				Layout.fillWidth: true
				placeholderText: qsTr("Enter issue ID")
				validator: IntValidator {
					bottom: 1
				}
				onAcceptableInputChanged: {

					docroot.addDialog.issueId = issueIdField.text
				}
			}

			TextField {
				id: timeSpentField
				Layout.fillWidth: true
				placeholderText: qsTr("Enter time spent (eg. 1.0)")
				validator: DoubleValidator {
					bottom: 0
					decimals: 2
					notation: DoubleValidator.StandardNotation
					locale: "C"
				}
				onAcceptableInputChanged: {

					if (text.length > 0 && acceptableInput) {
						let parsed = parseFloat(timeSpentField.text)
						console.log("Final parsed value:", parsed)
						docroot.addDialog.hours = parseFloat(parsed)
					}
				}
				onTextChanged: {
					console.log("Raw text:", text)
					console.log("Parsed value:", parseFloat(text))
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
