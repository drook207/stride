import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Effects

Item {
	anchors.fill: parent
	Drawer {
		id: drawer
		width: 200
		height: parent.height
		edge: Qt.RightEdge
		interactive: true
		modal: true
		Material.roundedScale: Material.NotRounded

		ColumnLayout {
			anchors {
				top: parent.top
				left: parent.left
				right: parent.right
			}
			spacing: 0

			Button {
				Layout.fillWidth: true
				Layout.preferredHeight: 80
				text: "Home"
				flat: true
				Material.roundedScale: Material.NotRounded
			}

			Rectangle {
				id: divider
				Layout.fillWidth: true
				Layout.preferredHeight: 2
				color: Material.frameColor

				MultiEffect {
					source: divider
					anchors.fill: divider
					shadowBlur: 1
					shadowEnabled: true
					shadowColor: "black"
					shadowVerticalOffset: 0
					shadowHorizontalOffset: 0
				}
			}
			Button {
				Layout.fillWidth: true
				Layout.preferredHeight: 80
				flat: true
				text: "Time Tracking"
				Material.roundedScale: Material.NotRounded
			}

			Rectangle {
				Layout.fillWidth: true
				Layout.preferredHeight: 4
				Layout.margins: 0
				color: Material.accentColor
			}
			Button {
				Layout.fillWidth: true
				Layout.preferredHeight: 80
				Layout.margins: 0
				flat: true
				text: "Settings"
				Material.roundedScale: Material.NotRounded
				padding: 0
			}
		}
	}

	Button {
		anchors {
			right: parent.right
			top: parent.top
		}
		icon.source: "qrc:/qt/qml/drook207/timeTracker/qml/Assets/Icons/solid/bars.svg"
		onClicked: drawer.open()
		Material.roundedScale: Material.NotRounded
		display: AbstractButton.IconOnly
	}
}
