import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Effects
import drook207.timeTracker
import drook207.redmineClient

ApplicationWindow {
	id: window
	visible: true
	width: 600
	height: 1024
	title: "Redmine Time Tracker"

	Material.theme: Material.Dark
	Material.accent: Material.LightGreen

	RedmineClient {
		id: redmineClient
		baseUrl: redmineClient.baseUrl === "" ? "https://your-redmine-instance.com" : redmineClient.baseUrl
		apiKey: redmineClient.apiKey === "" ? "your-api-key" : redmineClient.apiKey

		onErrorOccurred: function (error) {
			messageDialog.text = "Error: " + error
			messageDialog.open()
		}

		onTimeEntryCreated: function (timeEntry) {
			messageDialog.text = "Time entry created successfully"
			messageDialog.open()
		}
	}
	Component.onCompleted: {
		redmineClient.getEnumerations(0)
		redmineClient.getEnumerations(1)
		redmineClient.getEnumerations(2)
	}
	TimeTracker {
		id: timeTrackerInstance
	}
	TimeEntryActivitiesModel {
		id: activitiesModel
	}
	Connections {
		target: redmineClient
		function onTimeEntryActivitiesReceived(value) {
			activitiesModel.addTimeEntryActivities(value)
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
			timeTracker: timeTrackerInstance
		}

		BusyIndicator {
			running: redmineClient.loading
			Layout.alignment: Qt.AlignHCenter
		}
	}

	ConnectionStateLamp {
		anchors {
			left: parent.left
			bottom: parent.bottom
			margins: 10
		}
		connectionState: redmineClient.connectionState
	}

	BurgerMenu {}
}
