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
	//! The time entry custom field "Special" (Main.qml), or null if the server has none.
	property var specialField: null

	//! Activity to submit with the tracked time; 0 lets Redmine use its default.
	readonly property int selectedActivityId: activityCombo.activityId

	//! Height of every control in the bar (buttons, fields, combo boxes), so they line up.
	readonly property int controlHeight: Metrics.controlHeight
	//! Below this width the non-functional "Special" placeholder is hidden to keep room for the
	//! fields that matter.
	readonly property int compactWidth: 1100

	implicitHeight: layout.implicitHeight + layout.anchors.topMargin + layout.anchors.bottomMargin
	color: Material.dialogColor
	// Same corner radius as the Sidebar, which shares this surface color.
	radius: Metrics.panelRadius

	//! Whether the fields show the tracker's details yet; until then their changes aren't stored in
	//! timeTracker, so starting up can't overwrite the restored ones.
	property bool detailsRestored: false

	// A tracker restored from the previous run (see TimeTracker::restoreTrackingState) brings its
	// issue, comment and activity back into the fields.
	Component.onCompleted: {
		if (docroot.timeTracker.isTracking) {
			issueField.value = docroot.timeTracker.currentIssueId
			commentsField.text = docroot.timeTracker.trackingComment
			if (docroot.timeTracker.trackingActivityId > 0) {
				activityCombo.selectActivity(docroot.timeTracker.trackingActivityId)
			}
		}
		docroot.detailsRestored = true
	}

	Connections {
		target: commentsField
		function onTextChanged() {
			if (docroot.detailsRestored) {
				docroot.timeTracker.trackingComment = commentsField.text
			}
		}
	}
	// The tracker can also be started elsewhere (e.g. continuing a recent entry): show what it tracks.
	Connections {
		target: docroot.timeTracker
		function onCurrentIssueChanged() {
			if (docroot.timeTracker.isTracking && issueField.value !== docroot.timeTracker.currentIssueId) {
				issueField.value = docroot.timeTracker.currentIssueId
			}
		}
		// Submitted: ready for the next entry; the activity stays selected.
		function onTrackingCleared() {
			issueField.value = -1
			commentsField.clear()
			specialCombo.selectValue(undefined)
		}
		function onTrackingDetailsChanged() {
			if (commentsField.text !== docroot.timeTracker.trackingComment) {
				commentsField.text = docroot.timeTracker.trackingComment
			}
			if (docroot.timeTracker.trackingActivityId > 0 && docroot.selectedActivityId !== docroot.timeTracker.trackingActivityId) {
				activityCombo.selectActivity(docroot.timeTracker.trackingActivityId)
			}
		}
	}
	onSelectedActivityIdChanged: {
		if (docroot.detailsRestored) {
			docroot.timeTracker.trackingActivityId = docroot.selectedActivityId
		}
	}

	// Keyboard shortcuts for the buttons: start, pause or resume; submit.
	Shortcut {
		sequence: "Ctrl+Space"
		onActivated: {
			if (playButton.enabled) {
				playButton.clicked()
			} else if (pauseButton.enabled) {
				pauseButton.clicked()
			}
		}
	}
	Shortcut {
		sequences: ["Ctrl+Return", "Ctrl+Enter"]
		enabled: submitButton.enabled
		onActivated: submitButton.clicked()
	}

	Timer {
		id: inputValidatorTimer
		interval: 800
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
			id: playButton
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
				// Refresh the tracked issue, quietly: the user didn't ask for it.
				docroot.client.getIssue(issueField.value, true)
			}
		}

		RoundButton {
			id: pauseButton
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

		//! Kept in the layout while nothing is tracked (showing no time), so the bar doesn't jump.
		LabeledField {
			Layout.alignment: Qt.AlignBottom
			label: qsTr("Start")
			StartTimeField {
				Layout.preferredWidth: 150
				Layout.preferredHeight: docroot.controlHeight
				timeTracker: docroot.timeTracker
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
			ActivityComboBox {
				id: activityCombo
				Layout.fillWidth: true
				Layout.preferredHeight: docroot.controlHeight
				font.pixelSize: 13
				activitiesModel: docroot.activitiesModel
				projectId: issueField.projectId
			}
		}

		//! Redmine's "Special" time entry custom field. Hidden while the server has none, and below
		//! compactWidth; MobileTrackingBar doesn't offer it.
		LabeledField {
			Layout.alignment: Qt.AlignBottom
			Layout.fillWidth: true
			Layout.preferredWidth: 150
			Layout.minimumWidth: 110
			Layout.maximumWidth: 200
			visible: docroot.specialField !== null && docroot.width >= docroot.compactWidth
			label: qsTr("Special")
			SpecialFieldComboBox {
				id: specialCombo
				Layout.fillWidth: true
				Layout.preferredHeight: docroot.controlHeight
				font.pixelSize: 13
				field: docroot.specialField
			}
		}

		RoundButton {
			id: submitButton
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
				// Only sent while the field is shown, so a hidden selection never ends up on an entry.
				docroot.submitDialog.customFields = specialCombo.visible ? specialCombo.customFieldValues : []
				docroot.submitDialog.open()
			}
		}
	}
}
