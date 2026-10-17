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
	//! Redmine's projects ({id, name, ...}) to pick from; empty when they couldn't be loaded, in
	//! which case the project is entered by its ID instead.
	property var projects: []

	//! The selected or entered project ID, or -1 if there is none (yet).
	readonly property int projectId: docroot.projects.length > 0
									 ? (projectCombo.currentIndex >= 0 ? projectCombo.currentValue : -1)
									 : (projectIdField.acceptableInput ? parseInt(projectIdField.text) : -1)

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
					visible: docroot.projects.length > 0
					// The combo box's implicit width would otherwise squeeze the subject.
					Layout.preferredWidth: 220
					Layout.maximumWidth: 220
					label: qsTr("Project")

					ComboBox {
						id: projectCombo
						Layout.fillWidth: true
						Layout.preferredHeight: Metrics.controlHeight
						topInset: 0
						bottomInset: 0
						model: docroot.projects.slice().sort((a, b) => a.name.localeCompare(b.name))
						textRole: "name"
						valueRole: "id"
						currentIndex: -1
						displayText: currentIndex < 0 ? qsTr("Select a project") : currentText
					}
				}

				// Fallback while the project list isn't available (e.g. offline).
				LabeledField {
					visible: docroot.projects.length === 0
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
				enabled: subjectField.text.length > 0 && docroot.projectId > 0
				onClicked: docroot.client.createIssue(docroot.projectId, subjectField.text, descriptionField.text)
			}
		}
	}
}
