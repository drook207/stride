import QtQuick
import QtQuick.Controls
import QtQml

Rectangle {
	width: 300
	height: 400
	color: "white"

	Button {
		id: menuButton
		anchors {
			left: parent.left
			top: parent.top
			margins: 10
		}
		width: 40
		height: 40

		Image {
			anchors.centerIn: parent
			source: "qrc:/qml/Assets/Icons/solid/bars.svg" // Replace with your burger icon
			width: 24
			height: 24
		}

		onClicked: menu.visible = !menu.visible
	}

	Rectangle {
		id: menu
		width: 200
		height: parent.height
		color: "#f0f0f0"
		visible: false

		Column {
			anchors {
				top: parent.top
				topMargin: 60
				left: parent.left
				right: parent.right
			}
			spacing: 10

			MenuItem {
				text: "Home"
				onClicked: {
					menu.visible = false
					console.log("Home clicked")
				}
			}

			MenuItem {
				text: "Settings"
				onClicked: {
					menu.visible = false
					console.log("Settings clicked")
				}
			}

			MenuItem {
				text: "About"
				onClicked: {
					menu.visible = false
					console.log("About clicked")
				}
			}
		}
	}
}
// // MenuItem component
// Component {
// 	id: menuItemComponent

// 	Rectangle {
// 		property alias text: itemText.text
// 		signal clicked()

// 		width: parent.width
// 		height: 50
// 		color: mouseArea.pressed ? "#e0e0e0" : "transparent"

// 		Text {
// 			id: itemText
// 			anchors.verticalCenter: parent.verticalCenter
// 			x: 20
// 			font.pixelSize: 16
// 		}

// 		MouseArea {
// 			id: mouseArea
// 			anchors.fill: parent
// 			onClicked: parent.clicked()
// 		}
// 	}
// }
