import QtQuick
import QtQuick.Controls

/*!
 * \brief Combo box for the "Special" custom field of time entries (see Main.qml's specialField).
 *
 * Offers "None" plus the field's possible values. customFieldValues is what to send to Redmine
 * with a time entry (RedmineClient::createTimeEntry/updateTimeEntry). Disabled while the server
 * has no such field.
 */
ComboBox {
	id: combo

	//! The custom field as listed by RedmineClient::timeEntryCustomFieldsReceived, or null.
	property var field: null
	//! Whether nothing may be selected (shown as "Unchanged", nothing sent), for editing an entry
	//! whose value should stay as it is unless changed.
	property bool allowUnchanged: false

	//! [{id, value}] for the selected value ("" for None), or [] if nothing is selected.
	readonly property var customFieldValues: combo.field && combo.currentIndex >= 0
											 ? [{
													 "id": combo.field.id,
													 "value": combo.currentValue
												 }] : []

	//! Selects a value; undefined selects nothing (with allowUnchanged) or None.
	function selectValue(value) {
		combo.wantedValue = value
		combo.syncSelection()
	}

	property var wantedValue: undefined

	function syncSelection() {
		if (combo.wantedValue === undefined) {
			combo.currentIndex = combo.allowUnchanged ? -1 : 0
			return
		}
		const index = combo.indexOfValue(combo.wantedValue || "")
		combo.currentIndex = index >= 0 ? index : (combo.allowUnchanged ? -1 : 0)
	}

	model: [{
			"label": qsTr("None"),
			"value": ""
		}].concat(combo.field ? combo.field.possible_values.map(possible => ({
																		  "label": possible.label || possible.value,
																		  "value": possible.value
																	  })) : [])
	textRole: "label"
	valueRole: "value"
	displayText: combo.currentIndex < 0 ? qsTr("Unchanged") : combo.currentText
	enabled: combo.field !== null

	implicitHeight: Metrics.controlHeight
	topInset: 0
	bottomInset: 0

	// indexOfValue() only reflects a new model once it's processed, hence the deferred sync.
	onModelChanged: Qt.callLater(combo.syncSelection)
	Component.onCompleted: combo.syncSelection()
	onActivated: combo.wantedValue = combo.currentValue
}
