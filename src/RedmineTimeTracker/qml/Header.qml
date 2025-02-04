import QtQuick.Controls.Material
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
	id: header

	height: 40
	color: Material.backgroundColor

	Text {
		anchors {
			left: parent.left
			margins: 10
		}

		font.pixelSize: 20
		text: Application.name
		color: Material.foreground
	}

	Rectangle {
		id: separator
		anchors {
			left: parent.left
			right: parent.right
			bottom: parent.bottom
			margins: 10
		}

		height: 2
		color: Material.color(Material.frameColor)
	}

	MultiEffect {
		source: separator
		anchors.fill: separator
		shadowBlur: 0.1
		shadowEnabled: true
		shadowColor: "black"
		shadowVerticalOffset: 0
		shadowHorizontalOffset: 0
	}
}
