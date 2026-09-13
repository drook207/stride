import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
	id: labeledField

	property string label: ""

	spacing: 2

	Label {
		text: labeledField.label
		visible: labeledField.label.length > 0
		color: Material.hintTextColor
		font.pixelSize: 11
	}
}
