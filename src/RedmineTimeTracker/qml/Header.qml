import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QRedmine.Client

Rectangle {
	id: header

	required property int connectionState

	signal menuToggled

	height: 40
	color: Material.backgroundColor

	RoundButton {
		anchors {
			left: parent.left
			leftMargin: 10
			verticalCenter: parent.verticalCenter
		}
		flat: true
		icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/bars.svg"
		onClicked: header.menuToggled()
	}

	Text {
		anchors.centerIn: parent

		font.pixelSize: 20
		text: Application.name
		color: Material.foreground
	}

	RowLayout {
		anchors {
			right: parent.right
			rightMargin: 10
			verticalCenter: parent.verticalCenter
		}
		spacing: 10

		Rectangle {
			id: connectionPill
			radius: height / 2
			color: Qt.rgba(1, 1, 1, 0.08)
			implicitHeight: 24
			implicitWidth: pillContent.implicitWidth + 20

			RowLayout {
				id: pillContent
				anchors.centerIn: parent
				spacing: 6

				Rectangle {
					implicitWidth: 8
					implicitHeight: 8
					radius: 4
					color: {
						switch (header.connectionState) {
						case RedmineClient.ONLINE:
							return "#3DD68C"
						case RedmineClient.TEMPORARY_OFFLINE:
							return "#F5A623"
						default:
							return "#E5484D"
						}
					}
				}

				Label {
					text: {
						switch (header.connectionState) {
						case RedmineClient.ONLINE:
							return qsTr("Online")
						case RedmineClient.TEMPORARY_OFFLINE:
							return qsTr("Reconnecting")
						default:
							return qsTr("Offline")
						}
					}
					color: Material.foreground
					font.pixelSize: 11
				}
			}
		}

		RoundButton {
			flat: true
			icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/chevron-down.svg"
		}
	}
}
