import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QRedmine.Client

/*!
 * \brief Lets the user set an issue's status along with booking time on it, e.g. to "Resolved".
 *
 * Call load() with the issue; it asks Redmine for the statuses the user may set the issue to
 * (RedmineClient::getIssue with allowed statuses) and offers them, with "Unchanged" preselected.
 * Hidden when Redmine doesn't say (before 5.0) or allows no change. Pass statusId with the time
 * entry (TimeTracker::addRecentEntry's "issueStatusId"): it's set once the entry is on Redmine.
 */
ColumnLayout {
	id: picker

	required property RedmineClient client
	//! The issue loaded, -1 before.
	property int issueId: -1
	//! Status ids and names the issue may be set to, other than its current one.
	property var statuses: []
	property string currentStatus: ""
	//! The picked status, 0 for "Unchanged".
	readonly property int statusId: combo.currentIndex > 0 ? picker.statuses[combo.currentIndex - 1].id : 0

	function load(issueId) {
		picker.issueId = issueId
		picker.statuses = []
		picker.currentStatus = ""
		combo.currentIndex = 0
		if (issueId > 0) {
			picker.client.getIssue(issueId, true, true)
		}
	}

	visible: picker.statuses.length > 0
	spacing: 4

	Connections {
		target: picker.client
		function onIssueReceived(issue) {
			if (issue.id !== picker.issueId || !issue.allowed_statuses) {
				return
			}
			const current = issue.status ? issue.status.id : -1
			picker.currentStatus = issue.status ? issue.status.name : ""
			picker.statuses = issue.allowed_statuses.filter(status => status.id !== current)
		}
	}

	LabeledField {
		Layout.fillWidth: true
		label: qsTr("Issue status (currently %1)").arg(picker.currentStatus)

		ComboBox {
			id: combo
			Layout.fillWidth: true
			Layout.preferredHeight: Metrics.controlHeight
			topInset: 0
			bottomInset: 0
			model: [qsTr("Unchanged")].concat(picker.statuses.map(status => status.name))
		}
	}
}
