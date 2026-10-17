import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
 * \brief Dialog for editing an existing time entry's date, hours, activity and comment.
 *
 * Shared by RecentEntriesView and GetTimeEntriesView: call openFor() with the entry's current
 * values; on Save, saved() carries the changes and the caller decides where they go (Redmine
 * and/or the local database). The issue can't be changed.
 */
Dialog {
	id: dialog

	//! TimeEntryActivitiesModel for the Activity combo box.
	property var activitiesModel: undefined
	//! Issue of the entry being edited, shown in the title.
	property int issueId: -1
	//! The time entry custom field "Special" (Main.qml), or null if the server has none.
	property var specialField: null
	//! Project of the entry being edited, so only activities it allows are offered; -1 if unknown.
	property int projectId: -1
	//! Whether a local entry is on Redmine, as TimeEntryDelegate.syncState: "" for one that is (or
	//! an entry from Redmine), else "pending", "rejected" or "unconfirmed". Explains where the
	//! changes go.
	property string syncState: ""
	//! Why Redmine rejected the entry (syncState "rejected").
	property string syncError: ""

	//! Emitted on Save with "hours" (decimal hours), "comments", "spent_on" ("yyyy-MM-dd"), if one
	//! is selected "activity_id" and "activity_name", if changed "custom_fields", and "submit" when
	//! an unconfirmed entry is to be sent to Redmine.
	signal saved(var changes)

	//! The entry's hours as passed to openFor(). The field shows them rounded to 2 decimals, so
	//! they are saved as they were unless the field is edited.
	property real originalHours: 0

	//! Hours as entered, or NaN if the field doesn't hold a positive number.
	readonly property real hours: {
		if (hoursField.text === HoursFormat.format(dialog.originalHours)) {
			return dialog.originalHours
		}
		const value = HoursFormat.parse(hoursField.text)
		return value > 0 ? value : NaN
	}

	//! Opens the dialog for an entry.
	//! \param entry {issueId, projectId, hours, comments, spentOn ("yyyy-MM-dd..." or Date), activityId,
	//!              special (the Special field's value; undefined if unknown, keeping it unchanged),
	//!              syncState, syncError (see the properties; both optional)}
	function openFor(entry) {
		dialog.issueId = entry.issueId
		dialog.projectId = entry.projectId > 0 ? entry.projectId : -1
		dialog.syncState = entry.syncState || ""
		dialog.syncError = entry.syncError || ""
		sendCheck.checked = false
		dialog.originalHours = Number(entry.hours)
		hoursField.text = HoursFormat.format(dialog.originalHours)
		commentsField.text = entry.comments || ""
		const spentOn = entry.spentOn instanceof Date ? entry.spentOn : new Date(String(entry.spentOn).substring(0, 10) + "T00:00:00")
		dateField.selectedDate = isNaN(spentOn.getTime()) ? new Date() : spentOn
		timeRange.clear()
		activityCombo.selectActivity(entry.activityId > 0 ? entry.activityId : 0)
		specialCombo.selectValue(entry.special)
		dialog.open()
	}

	parent: Overlay.overlay
	anchors.centerIn: parent
	width: Math.min(parent.width - 32, 560)
	modal: true
	Overlay.modal: DialogScrim {}

	title: qsTr("Edit time entry for #%1").arg(dialog.issueId)
	standardButtons: Dialog.Save | Dialog.Cancel

	Component.onCompleted: standardButton(Dialog.Save).enabled = Qt.binding(() => !isNaN(dialog.hours))

	onAccepted: {
		const changes = {
			"hours": dialog.hours,
			"comments": commentsField.text,
			"spent_on": dateField.isoDate
		}
		if (activityCombo.activityId > 0) {
			changes["activity_id"] = activityCombo.activityId
			changes["activity_name"] = activityCombo.currentText
		}
		if (specialCombo.customFieldValues.length > 0) {
			changes["custom_fields"] = specialCombo.customFieldValues
		}
		if (dialog.syncState === "unconfirmed" && sendCheck.checked) {
			changes["submit"] = true
		}
		dialog.saved(changes)
	}

	contentItem: ColumnLayout {
		spacing: 12

		Label {
			Layout.fillWidth: true
			visible: dialog.syncState !== ""
			text: {
				switch (dialog.syncState) {
				case "pending":
					return qsTr("This entry isn't on Redmine yet. It's sent with your changes once Redmine is reachable.")
				case "rejected":
					return qsTr("Redmine rejected this entry: %1\nSaving sends it again.").arg(dialog.syncError)
				default:
					return qsTr("This entry was saved before the app kept track of sending entries, so it may already be on Redmine. Changes are only saved in the app, unless you send it.")
				}
			}
			color: dialog.syncState === "rejected" ? Material.color(Material.Red, Material.Shade200) : Material.secondaryTextColor
			wrapMode: Text.WordWrap
		}

		CheckBox {
			id: sendCheck
			Layout.fillWidth: true
			visible: dialog.syncState === "unconfirmed"
			text: qsTr("It's not on Redmine yet - send it when saving")
		}

		LabeledField {
			Layout.fillWidth: true
			label: qsTr("Date")

			DatePickerField {
				id: dateField
				Layout.fillWidth: true
				Layout.preferredHeight: Metrics.controlHeight
			}
		}

		TimeRangeRow {
			id: timeRange
			Layout.fillWidth: true
			onDurationPicked: hours => hoursField.text = HoursFormat.format(hours)
		}

		RowLayout {
			Layout.fillWidth: true
			spacing: 12

			LabeledField {
				Layout.fillWidth: true
				label: qsTr("Time spent (eg. 1,0)")

				TextField {
					id: hoursField
					Layout.fillWidth: true
					Layout.preferredHeight: Metrics.controlHeight
				}
			}

			LabeledField {
				Layout.fillWidth: true
				label: qsTr("Activity")

				ActivityComboBox {
					id: activityCombo
					Layout.fillWidth: true
					Layout.preferredHeight: Metrics.controlHeight
					activitiesModel: dialog.activitiesModel
					projectId: dialog.projectId
					// Nothing preselected: -1 keeps the entry's activity unchanged.
					selectDefault: false
					displayText: currentIndex < 0 ? qsTr("Unchanged") : currentText
				}
			}
		}

		// Redmine's "Special" time entry custom field.
		LabeledField {
			Layout.fillWidth: true
			visible: dialog.specialField !== null
			label: qsTr("Special")

			SpecialFieldComboBox {
				id: specialCombo
				Layout.fillWidth: true
				Layout.preferredHeight: Metrics.controlHeight
				field: dialog.specialField
				// Nothing preselected for an unknown value: keeps it unchanged.
				allowUnchanged: true
			}
		}

		LabeledField {
			Layout.fillWidth: true
			label: qsTr("Comments")

			TextArea {
				id: commentsField
				Layout.fillWidth: true
				Layout.preferredHeight: Metrics.textAreaHeight
				wrapMode: TextEdit.Wrap
			}
		}
	}
}
