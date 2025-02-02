import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
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
					Layout.preferredHeight: 80
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
							right: parent.right
							bottom: parent.bottom
						}
						height: 4
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
				}
			}
		}
	}

	Button {
		anchors {
			right: parent.right
			top: parent.top
		}
		icon.width: 5
		icon.source: "qrc:/qt/qml/drook207/timeTracker/qml/Assets/Icons/solid/ellipsis-vertical.svg"
		onClicked: drawer.open()
		Material.roundedScale: Material.NotRounded
		display: AbstractButton.IconOnly
	}
}
