import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Item {
	id: burgerMenu
	anchors.fill: parent

	required property StackView view
	required property var componentsToLoad
	required property var buttonTitles

	Drawer {
		id: drawer
		width: 200
		height: parent.height
		edge: Qt.RightEdge
		interactive: true
		modal: true
		dim: false

		background: Rectangle {
			color: Material.dialogColor

			border {
				color: "black"
				width: 1
			}

			topLeftRadius: 5
			bottomLeftRadius: 5
		}

		Material.roundedScale: Material.NotRounded
		ColumnLayout {
			anchors {
				top: parent.top
				left: parent.left
				right: parent.right
			}
			spacing: 0

			Repeater {
				model: burgerMenu.buttonTitles

				Button {
					required property int index

					Layout.fillWidth: true
					Layout.preferredHeight: 35
					text: burgerMenu.buttonTitles[index]
					flat: true
					Material.roundedScale: Material.NotRounded

					onClicked: {
						burgerMenu.view.replaceCurrentItem(burgerMenu.componentsToLoad[index])
					}

					Rectangle {
						id: divider
						visible: parent.index != (burgerMenu.componentsToLoad.count - 1)
						anchors {
							left: parent.left
							leftMargin: 2
							right: parent.right
							rightMargin: 2
							bottom: parent.bottom
						}
						height: 2
						color: Material.frameColor
					}
				}
			}
		}
	}

	RoundButton {
		id: menuButton
		anchors {
			right: parent.right
			rightMargin: drawer.width * drawer.position + 5
			top: parent.top
			topMargin: 0
		}
		padding: 0
		width: 35
		height: 35
		icon {
			width: 5
			height: menuButton.height / 2
			source: "qrc:/qt/qml/drook207/timeTracker/qml/Assets/Icons/solid/ellipsis-vertical.svg"
		}
		onClicked: drawer.open()
		display: AbstractButton.IconOnly
	}
}
