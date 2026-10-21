import QtQuick
import QtQuick.Controls

/*!
 * \brief Combo box for the "Special" custom field of time entries (see Main.qml's specialField).
 *
 * Offers "None" plus the field's possible values. customFieldValues is what to send to Redmine
 * with a time entry (RedmineClient::createTimeEntry/updateTimeEntry). Disabled while the server
 * has no such field, or only one without values to choose from (e.g. a text field).
 */
ComboBox {
	id: combo

	//! The custom field as listed by RedmineClient::timeEntryCustomFieldsReceived, or null.
	property var field: null
	//! Whether nothing may be selected (shown as "Unchanged", nothing sent), for editing an entry
	//! whose value should stay as it is unless changed.
	property bool allowUnchanged: false

	//! Whether field has values to offer: Redmine lists possible_values only for list-like formats
	//! (list, key/value list, boolean), not e.g. for string or text fields.
	readonly property bool usable: !!combo.field && Array.isArray(combo.field.possible_values)

	//! [{id, value}] for the selected value ("" for None), or [] if nothing is selected.
	readonly property var customFieldValues: combo.usable && combo.currentIndex >= 0
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
		}].concat(combo.usable ? combo.field.possible_values.map(possible => ({
																		  "label": possible.label || possible.value,
																		  "value": possible.value
																	  })) : [])
	textRole: "label"
	valueRole: "value"
	displayText: combo.currentIndex < 0 ? qsTr("Unchanged") : combo.currentText
	enabled: combo.usable

	implicitHeight: Metrics.controlHeight
	topInset: 0
	bottomInset: 0

	// indexOfValue() only reflects a new model once it's processed, hence the deferred sync.
	onModelChanged: Qt.callLater(combo.syncSelection)
	Component.onCompleted: combo.syncSelection()
	onActivated: combo.wantedValue = combo.currentValue
}
