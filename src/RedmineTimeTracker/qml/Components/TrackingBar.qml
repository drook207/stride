import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief Desktop bottom control bar for live time tracking: play/pause, elapsed time, issue
 * search, comment, activity, and submit, all shown at once in a single wide row.
 *
 * Functionally equivalent to MobileTrackingBar, which instead collapses everything but
 * play/pause and the elapsed time by default - there's no room for a single-row layout like this
 * one on a phone screen.
 */
Rectangle {
	id: docroot

	required property TimeTracker timeTracker
	required property RedmineClient client
	required property SubmitDialog submitDialog
	required property var activitiesModel

	property int selectedActivityId: 0

	//! Height of every control in the bar (buttons, fields, combo boxes), so they line up.
	readonly property int controlHeight: 40
	//! Below this width the non-functional "Special" placeholder is hidden to keep room for the
	//! fields that matter.
	readonly property int compactWidth: 1100

	implicitHeight: layout.implicitHeight + layout.anchors.topMargin + layout.anchors.bottomMargin
	color: Material.dialogColor
	// Same corner radius as the Sidebar, which shares this surface color.
	radius: 12

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
		id: layout
		anchors {
			fill: parent
			leftMargin: 16
			rightMargin: 16
			topMargin: 10
			bottomMargin: 12
		}
		spacing: 12

		// Buttons have no label, so every child is bottom-aligned to line the buttons up with the
		// labeled fields' inputs.

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
			Layout.alignment: Qt.AlignBottom
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

		LabeledField {
			Layout.alignment: Qt.AlignBottom
			label: qsTr("Time")
			TextField {
				Layout.preferredWidth: 96
				Layout.preferredHeight: docroot.controlHeight
				leftPadding: 8
				rightPadding: 8
				text: docroot.timeTracker.elapsedTime
				readOnly: true
				color: Material.foreground
				font.pixelSize: 14
				font.bold: true
				horizontalAlignment: Text.AlignHCenter
			}
		}

		LabeledField {
			Layout.alignment: Qt.AlignBottom
			Layout.fillWidth: true
			Layout.preferredWidth: 160
			Layout.minimumWidth: 110
			Layout.maximumWidth: 220
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
			Layout.alignment: Qt.AlignBottom
			Layout.fillWidth: true
			Layout.horizontalStretchFactor: 3
			Layout.preferredWidth: 300
			Layout.minimumWidth: 140
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

		LabeledField {
			Layout.alignment: Qt.AlignBottom
			Layout.fillWidth: true
			Layout.preferredWidth: 170
			Layout.minimumWidth: 120
			Layout.maximumWidth: 240
			label: qsTr("Activity")
			ComboBox {
				id: activityCombo
				Layout.fillWidth: true
				Layout.preferredHeight: docroot.controlHeight
				topInset: 0
				bottomInset: 0
				font.pixelSize: 13
				model: docroot.activitiesModel.timeEntryActivities
				textRole: "name"
				valueRole: "id"
				onActivated: docroot.selectedActivityId = activityCombo.currentValue
			}
		}

		//! Known non-functional placeholder: empty model, nothing reads its selection. Not
		//! rendered by MobileTrackingBar, which otherwise mirrors this bar's fields, and hidden here
		//! below compactWidth.
		LabeledField {
			Layout.alignment: Qt.AlignBottom
			Layout.fillWidth: true
			Layout.preferredWidth: 150
			Layout.minimumWidth: 110
			Layout.maximumWidth: 200
			visible: docroot.width >= docroot.compactWidth
			label: qsTr("Special")
			ComboBox {
				id: specialCombo
				Layout.fillWidth: true
				Layout.preferredHeight: docroot.controlHeight
				topInset: 0
				bottomInset: 0
				font.pixelSize: 13
				model: []
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
				docroot.submitDialog.open()
			}
		}
	}
}
