import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import drook207.timeTracker
import drook207.redmineClient

Dialog {
	id: docroot
	title: "Add Time Entry"
	standardButtons: Dialog.Yes | Dialog.No
	x: (parent.width - width) / 2
	y: (parent.height - height) / 2
	parent: Overlay.overlay
	modal: true
	property TimeTracker timeTracker: undefined
	property RedmineClient client: undefined
	property string issueId: ""
	property string comment: ""
	property string spentOn: ""
	property double hours: 0.0

	onAccepted: {

		const hours = parseFloat(docroot.hours)
		const issueId = parseInt(docroot.issueId)
		const comments = docroot.comment
		const spentOn = docroot.spentOn === "" ? new Date() : Date.fromLocaleDateString(
													 docroot.spentOn)
		// Add to local history
		docroot.timeTracker.addRecentEntry({
											   "issueId": issueId,
											   "hours": hours,
											   "comments": comments,
											   "date": spentOn
										   })

		// Submit to Redmine
		docroot.client.createTimeEntry(issueId, hours, comments,
									   docroot.spentOn)
	}

	onRejected: {

	}

	contentItem: ColumnLayout {
		spacing: 10

		Label {
			text: "Would you like to submit this time entry?"
		}

		Label {
			text: "Total time: " + docroot.hours + " spent on: " + docroot.spentOn
			font.bold: true
		}
	}
}
