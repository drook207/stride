import QtQuick
import QtQuick.Controls

Dialog {
	id: messageDialog
	property alias text: messageLabel.text

	parent: Overlay.overlay
	width: parent.width * 0.8
	height: messageLabel.implicitHeight + 200

	title: "Message"
	standardButtons: Dialog.Ok
	x: (parent.width - width) / 2
	y: (parent.height - height) / 2
	modal: true

	contentItem: Label {
		id: messageLabel
		wrapMode: Text.WordWrap
	}
}
