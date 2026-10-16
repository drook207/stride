import QtQuick
import QtQuick.Controls

/*!
 * \brief Generic single-button (OK) popup for showing a message or error to the user.
 *
 * Callers just set `text` and call open() - e.g. Main.qml uses this for RedmineClient's
 * errorOccurred/timeEntryCreated notifications, and AddIssueView for issueCreated.
 */
Dialog {
	id: messageDialog
	//! The message to display; set this then call open().
	property alias text: messageLabel.text

	parent: Overlay.overlay
	width: parent.width * 0.8
	height: messageLabel.implicitHeight + 200

	title: qsTr("Message")
	standardButtons: Dialog.Ok
	x: (parent.width - width) / 2
	y: (parent.height - height) / 2
	modal: true
	Overlay.modal: DialogScrim {}

	contentItem: Label {
		id: messageLabel
		wrapMode: Text.WordWrap
	}
}
