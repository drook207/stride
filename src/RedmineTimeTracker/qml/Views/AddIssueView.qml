import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

Item {
	id: docroot

	required property RedmineClient client

	Connections {
		target: docroot.client
		function onIssueCreated(issue) {
			messageDialog.text = qsTr("Issue #%0 created successfully").arg(issue.id)
			messageDialog.open()
		}
	}

	MessageDialog {
		id: messageDialog
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
			text: qsTr("Add Issue")
			font.bold: true
			font.pixelSize: 18
			color: Material.foreground
		}

		ColumnLayout {
			Layout.fillWidth: true
			spacing: 10

			LabeledField {
				Layout.fillWidth: true
				label: qsTr("Project ID")

				TextField {
					id: projectIdField
					Layout.fillWidth: true
					validator: IntValidator {
						bottom: 1
					}
				}
			}

			LabeledField {
				Layout.fillWidth: true
				label: qsTr("Subject")

				TextField {
					id: subjectField
					Layout.fillWidth: true
				}
			}
		}

		LabeledField {
			Layout.fillWidth: true
			label: qsTr("Description (optional)")

			TextArea {
				id: descriptionField
				Layout.fillWidth: true
				Layout.preferredHeight: 100
				wrapMode: TextEdit.Wrap
			}
		}

		RoundButton {
			text: qsTr("Add Issue")
			highlighted: true
			enabled: subjectField.text.length > 0 && projectIdField.acceptableInput
			onClicked: docroot.client.createIssue(parseInt(projectIdField.text), subjectField.text, descriptionField.text)
		}
	}
}
