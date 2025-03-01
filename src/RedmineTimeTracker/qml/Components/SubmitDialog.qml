import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import drook207.timeTracker
import drook207.redmineClient

Dialog {
	id: docroot
	width: parent.width * 0.8
	height: content.implicitHeight + 200

	required property TimeTracker timeTracker
	required property RedmineClient client
	property string issueId: ""
	property string comment: ""
	property string spentOn: ""

	title: qsTr("Submit Time Entry")
	standardButtons: Dialog.Yes | Dialog.No
	anchors.centerIn: parent
	parent: Overlay.overlay
	modal: true

	onAccepted: {
		docroot.timeTracker.stopTracking()

		const hours = docroot.timeTracker.getElapsedHours()
		const issueId = parseInt(docroot.issueId)
		const comments = docroot.comment
		const spentOn = docroot.spentOn === "" ? new Date() : Date.fromLocaleDateString(spentOn)
		console.log("Issue ID: " + issueId)
		// Add to local history
		docroot.timeTracker.addRecentEntry({
											   "issueId": issueId,
											   "hours": hours,
											   "comments": comments,
											   "date": spentOn
										   })

		// Submit to Redmine
		docroot.client.createTimeEntry(issueId, hours, comments)
		docroot.comment.text = ""
	}

	onRejected: {
		docroot.timeTracker.stopTracking()
		docroot.comment.text = ""
	}

	contentItem: ColumnLayout {
		id: content
		spacing: 10

		Label {
			text: qsTr("Would you like to submit this time entry?")
		}

		Label {
			text: qsTr("Total time: %0").arg(docroot.timeTracker.elapsedTime)
			font.bold: true
		}
	}
}
