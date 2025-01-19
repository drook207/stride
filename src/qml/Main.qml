// main.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import drook207.timeTracker 1.0

ApplicationWindow {
	id: window
	visible: true
	width: 800
	height: 600
	title: "Redmine Time Tracker"

	RedmineClient {
		id: redmineClient
		baseUrl: "https://your-redmine-instance.com"
		apiKey: "your-api-key"

		onErrorOccurred: function (error) {
			messageDialog.text = "Error: " + error
			messageDialog.open()
		}

		onTimeEntryCreated: function (timeEntry) {
			messageDialog.text = "Time entry created successfully"
			messageDialog.open()
		}
	}

	Dialog {
		id: messageDialog
		title: "Message"
		standardButtons: Dialog.Ok
		x: (parent.width - width) / 2
		y: (parent.height - height) / 2
		parent: Overlay.overlay
		modal: true

		property alias text: messageLabel.text

		contentItem: Label {
			id: messageLabel
			wrapMode: Text.WordWrap
		}
	}

	ColumnLayout {
		anchors.fill: parent
		anchors.margins: 10
		spacing: 10

		GroupBox {
			title: "Connection Settings"
			Layout.fillWidth: true

			GridLayout {
				columns: 2
				rowSpacing: 10
				columnSpacing: 10
				anchors.fill: parent

				Label {
					text: "Redmine URL:"
				}
				TextField {
					Layout.fillWidth: true
					text: redmineClient.baseUrl
					onTextChanged: redmineClient.baseUrl = text
					placeholderText: "Enter your Redmine URL"
				}

				Label {
					text: "API Key:"
				}
				TextField {
					Layout.fillWidth: true
					text: redmineClient.apiKey
					onTextChanged: redmineClient.apiKey = text
					echoMode: TextInput.Password
					placeholderText: "Enter your API key"
				}
			}
		}

		TimeTrackerView {
			Layout.fillWidth: true
			Layout.fillHeight: true
			client: redmineClient
		}

		BusyIndicator {
			running: redmineClient.loading
			Layout.alignment: Qt.AlignHCenter
		}
	}
}
