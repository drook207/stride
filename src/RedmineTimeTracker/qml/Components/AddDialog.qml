import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief Confirmation dialog for AddTimeEntryView's manual "Add" flow.
 *
 * The caller sets issueId/comment/spentOn/hours before calling open(); on accept, the entry is
 * added to the local history and submitted to Redmine. Deliberately similar to SubmitDialog (same
 * confirmation UX), but takes its values as plain properties rather than reading them live from
 * TimeTracker, since this flow isn't tied to a running tracked session.
 */
Dialog {
	id: docroot
	parent: Overlay.overlay
	width: parent.width * 0.8
	height: content.implicitHeight + 200

	title: qsTr("Add Time Entry")
	standardButtons: Dialog.Yes | Dialog.No
	x: (parent.width - width) / 2
	y: (parent.height - height) / 2
	modal: true
	Overlay.modal: DialogScrim {}
	property TimeTracker timeTracker: undefined
	property RedmineClient client: undefined
	property string issueId: ""
	property string comment: ""
	property string spentOn: ""
	property double hours: 0.0
	//! Redmine time entry activity id; 0 lets Redmine use its default activity.
	property int activityId: 0
	//! Custom field values to submit ([{id, value}]), see SpecialFieldComboBox.
	property var customFields: []

	onAccepted: {

		const hoursResult = parseFloat(docroot.hours)
		const issueIdResult = docroot.issueId
		const commentsResult = docroot.comment
		const spentOnResult = docroot.spentOn === "" ? new Date() : docroot.spentOn
		console.log("Add dialog hours value: ", hoursResult)
		console.log("Add dialog issueId value: ", issueIdResult)
		console.log("Add dialog comment value: ", commentsResult)
		console.log("Add dialog spentOn value: ", spentOnResult)
		// Add to local history; its id lets the entry be linked to the created time entry.
		const localId = docroot.timeTracker.addRecentEntry({
											   "issueId": issueIdResult,
											   "hours": hoursResult,
											   "comments": commentsResult,
											   "date": spentOnResult
										   })

		// Submit to Redmine
		docroot.client.createTimeEntry(issueIdResult, hoursResult, commentsResult, spentOnResult, docroot.activityId, docroot.customFields, localId)
	}

	onRejected: {

	}

	contentItem: ColumnLayout {
		id: content
		spacing: 10

		Label {
			text: qsTr("Would you like to submit this time entry?")
		}

		Label {
			text: qsTr("Total time: %1 spent on: %2").arg(docroot.hours).arg(docroot.spentOn)
			font.bold: true
		}
	}
}
