import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief Confirmation dialog for the live tracking submit flow (TrackingBar/MobileTrackingBar).
 *
 * The caller sets issueId/comment/activityId before calling open(); on accept, this stops the
 * running tracker, reads the elapsed time from it directly, adds the entry to the local history,
 * and submits it to Redmine. Contrast with AddDialog, which takes hours/spentOn as plain
 * properties instead since its flow isn't tied to a running tracked session.
 */
Dialog {
	id: docroot
	width: parent.width * 0.8
	height: content.implicitHeight + 200

	required property TimeTracker timeTracker
	required property RedmineClient client
	property string issueId: ""
	property string comment: ""
	property string spentOn: ""
	property int activityId: 0

	title: qsTr("Submit Time Entry")
	standardButtons: Dialog.Yes | Dialog.No
	anchors.centerIn: parent
	parent: Overlay.overlay
	modal: true
	Overlay.modal: DialogScrim {}

	onAccepted: {
		docroot.timeTracker.stopTracking()

		const hours = docroot.timeTracker.getElapsedHours()
		const issueId = parseInt(docroot.issueId)
		const comments = docroot.comment
		const spentOn = docroot.spentOn === "" ? new Date() : Date.fromLocaleDateString(spentOn)
		console.log("Issue ID: " + issueId)
		// Add to local history; its id lets the entry be linked to the created time entry.
		const localId = docroot.timeTracker.addRecentEntry({
											   "issueId": issueId,
											   "hours": hours,
											   "comments": comments,
											   "date": spentOn
										   })

		// Submit to Redmine
		docroot.client.createTimeEntry(issueId, hours, comments, docroot.spentOn === "" ? Qt.formatDate(new Date(), "yyyy-MM-dd") : docroot.spentOn, docroot.activityId, localId)
		docroot.comment = ""
	}

	onRejected: {
		docroot.timeTracker.stopTracking()
		docroot.comment = ""
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
