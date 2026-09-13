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

	implicitHeight: 84
	color: Material.dialogColor

	Timer {
		id: inputValidatorTimer
		interval: 500
		onTriggered: {
			if (issueField.acceptableInput) {
				docroot.client.getIssue(issueField.value)
			}
		}
	}

	RowLayout {
		anchors {
			fill: parent
			leftMargin: 15
			rightMargin: 15
			topMargin: 10
			bottomMargin: 10
		}
		spacing: 15

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

		LabeledField {
			TextField {
				Layout.preferredWidth: 90
				Layout.preferredHeight: 40
				text: docroot.timeTracker.elapsedTime
				readOnly: true
				color: Material.foreground
				font.pixelSize: 15
				font.bold: true
				horizontalAlignment: Text.AlignHCenter
			}
		}

		LabeledField {
			label: qsTr("Issue")
			RowLayout {
				spacing: 5
				IssueSearchInputField {
					id: issueField
					Layout.preferredWidth: 150
					Layout.preferredHeight: 40
					timeTracker: docroot.timeTracker

					onAccepted: {
						docroot.client.getIssue(issueField.value)
						inputValidatorTimer.stop()
					}

					onTextChanged: inputValidatorTimer.restart()
				}
				ToolButton {
					Layout.preferredWidth: 40
					Layout.preferredHeight: 40
					icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/xmark.svg"
					onClicked: issueField.value = -1
				}
			}
		}

		LabeledField {
			Layout.fillWidth: true
			label: qsTr("Comment")
			RowLayout {
				Layout.fillWidth: true
				spacing: 5
				TextField {
					id: commentsField
					Layout.fillWidth: true
					Layout.preferredHeight: 40
					font.pixelSize: 13
				}
				ToolButton {
					Layout.preferredWidth: 40
					Layout.preferredHeight: 40
					icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/xmark.svg"
					onClicked: commentsField.text = ""
				}
			}
		}

		LabeledField {
			label: qsTr("Activity")
			ComboBox {
				id: activityCombo
				Layout.preferredWidth: 140
				Layout.preferredHeight: 40
				font.pixelSize: 13
				model: docroot.activitiesModel.timeEntryActivities
				textRole: "name"
				valueRole: "id"
				background: Item {}
				indicator: Image {
					x: activityCombo.width - width - activityCombo.rightPadding
					y: activityCombo.topPadding + (activityCombo.availableHeight - height) / 2
					source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/chevron-up.svg"
					sourceSize.width: 12
					sourceSize.height: 12
				}
				onActivated: docroot.selectedActivityId = activityCombo.currentValue
			}
		}

		LabeledField {
			label: qsTr("Special")
			ComboBox {
				id: specialCombo
				Layout.preferredWidth: 120
				Layout.preferredHeight: 40
				font.pixelSize: 13
				model: []
				background: Item {}
				indicator: Image {
					x: specialCombo.width - width - specialCombo.rightPadding
					y: specialCombo.topPadding + (specialCombo.availableHeight - height) / 2
					source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/chevron-up.svg"
					sourceSize.width: 12
					sourceSize.height: 12
				}
			}
		}

		RoundButton {
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
