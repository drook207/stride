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

		const hoursResult = parseFloat(docroot.hours)
		const issueIdResult = docroot.issueId
		const commentsResult = docroot.comment
		const spentOnResult = docroot.spentOn === "" ? new Date() : docroot.spentOn
		console.log("Add dialog hours value: ", hoursResult)
		console.log("Add dialog issueId value: ", issueIdResult)
		console.log("Add dialog comment value: ", commentsResult)
		console.log("Add dialog spentOn value: ", spentOnResult)
		// Add to local history
		docroot.timeTracker.addRecentEntry({
											   "issueId": issueIdResult,
											   "hours": hoursResult,
											   "comments": commentsResult,
											   "date": spentOnResult
										   })

		// Submit to Redmine
		docroot.client.createTimeEntry(issueIdResult, hoursResult, commentsResult, spentOnResult)
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
