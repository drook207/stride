import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

/*!
 * \brief Small, unobtrusive "clear" (x) button placed inside the right edge of an input field.
 *
 * Usage: declare it as a child of the field, anchor it to the field's right edge and give the
 * field enough rightPadding so typed text doesn't run underneath it, e.g.
 * `TextField { rightPadding: 36; ClearButton { anchors.right: parent.right; ... } }`.
 */
ToolButton {
	implicitWidth: 28
	implicitHeight: 28
	padding: 0
	topInset: 0
	bottomInset: 0
	leftInset: 0
	rightInset: 0

	icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/xmark.svg"
	icon.width: 12
	icon.height: 12
	icon.color: Material.secondaryTextColor

	// Clearing is a mouse shortcut; keyboard users clear the field's text directly.
	focusPolicy: Qt.NoFocus
}
