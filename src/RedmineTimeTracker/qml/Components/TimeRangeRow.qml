import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/*!
 * \brief Optional From/To time pickers that work out a duration, e.g. 10:00-13:30 = 3.5 hours.
 *
 * Emits durationPicked() once both times are set (again on every change); an end before the
 * start is taken as spanning midnight (22:00-01:00 = 3 hours). Redmine only stores the hours, so
 * callers use the duration to fill in their hours field rather than submitting the range.
 */
RowLayout {
	id: row

	//! Emitted with the picked range's length in decimal hours, whenever both times are set.
	signal durationPicked(real hours)

	//! Empties both pickers.
	function clear() {
		fromField.hasTime = false
		toField.hasTime = false
	}

	function emitDuration() {
		if (!fromField.hasTime || !toField.hasTime) {
			return
		}
		let minutes = toField.totalMinutes - fromField.totalMinutes
		if (minutes < 0) {
			minutes += 24 * 60
		}
		if (minutes > 0) {
			row.durationPicked(minutes / 60)
		}
	}

	spacing: 12

	LabeledField {
		Layout.fillWidth: true
		label: qsTr("From")

		TimePickerField {
			id: fromField
			Layout.fillWidth: true
			Layout.preferredHeight: Metrics.controlHeight
			onTimePicked: row.emitDuration()
		}
	}

	LabeledField {
		Layout.fillWidth: true
		label: qsTr("To")

		TimePickerField {
			id: toField
			Layout.fillWidth: true
			Layout.preferredHeight: Metrics.controlHeight
			// Suggest one hour after the start time.
			initialMinutes: fromField.hasTime ? fromField.totalMinutes + 60 : -1
			onTimePicked: row.emitDuration()
		}
	}
}
