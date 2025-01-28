import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import drook207.timeTracker
import drook207.redmineClient

Dialog {
	id: docroot
	title: "Submit Time Entry"
	standardButtons: Dialog.Yes | Dialog.No
	x: (parent.width - width) / 2
	y: (parent.height - height) / 2
	parent: Overlay.overlay
	modal: true
	property TimeTracker timeTracker: TimeTracker {}
	property RedmineClient client: undefined
	property string issueId: ""
	property string comment: ""
	property string spentOn: ""

	onAccepted: {
		docroot.timeTracker.stopTracking()

		const hours = docroot.timeTracker.getElapsedHours()
		const issueId = parseInt(docroot.issueId)
		const comments = docroot.comment
		const spentOn = docroot.spentOn === "" ? new Date() : Date.fromLocaleDateString(
													 spentOn)
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
		spacing: 10

		Label {
			text: "Would you like to submit this time entry?"
		}

		Label {
			text: "Total time: " + docroot.timeTracker.elapsedTime
			font.bold: true
		}
	}
}
