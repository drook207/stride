import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief "Add Issue" view: creates a new Redmine issue from a project ID, subject, and optional
 * description.
 */
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

	FormPage {
		anchors.fill: parent
		title: qsTr("Add Issue")

		Card {
			Layout.fillWidth: true

			RowLayout {
				Layout.fillWidth: true
				spacing: 12

				LabeledField {
					// Only holds a number; the Material field's implicit width would otherwise make
					// it as wide as the subject.
					Layout.preferredWidth: 140
					Layout.maximumWidth: 140
					label: qsTr("Project ID")

					TextField {
						id: projectIdField
						Layout.fillWidth: true
						Layout.preferredHeight: Metrics.controlHeight
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
						Layout.preferredHeight: Metrics.controlHeight
					}
				}
			}

			LabeledField {
				Layout.fillWidth: true
				label: qsTr("Description (optional)")

				TextArea {
					id: descriptionField
					Layout.fillWidth: true
					Layout.preferredHeight: Metrics.textAreaHeight
					wrapMode: TextEdit.Wrap
				}
			}

			PrimaryButton {
				Layout.alignment: Qt.AlignRight
				Layout.topMargin: 4
				text: qsTr("Add Issue")
				enabled: subjectField.text.length > 0 && projectIdField.acceptableInput
				onClicked: docroot.client.createIssue(parseInt(projectIdField.text), subjectField.text, descriptionField.text)
			}
		}
	}
}
