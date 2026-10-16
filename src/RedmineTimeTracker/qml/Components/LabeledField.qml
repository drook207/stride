import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
 * \brief Wraps a single child control with a small hint-styled label above it.
 *
 * Usage: `LabeledField { label: qsTr("..."); TextField { ... } }`. Intended for exactly one child
 * control - it's a plain ColumnLayout, so nothing enforces that, but multiple children will just
 * stack under the label rather than sit beside it.
 */
ColumnLayout {
	id: labeledField

	//! Hint text shown above the child control; hidden entirely when empty (see TrackingBar's
	//! elapsed-time field, which omits it for a label-less display).
	property string label: ""

	spacing: 2

	Label {
		text: labeledField.label
		visible: labeledField.label.length > 0
		color: Material.hintTextColor
		font.pixelSize: 11
	}
}
