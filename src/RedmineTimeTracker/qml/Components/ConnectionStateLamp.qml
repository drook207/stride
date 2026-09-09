import QtQuick
import QtQuick.Effects
import QRedmine.Client

Item {
	id: connectionStateLamp
	property int connectionState: RedmineClient.OFFLINE
	width: 20
	height: 20

	Rectangle {
		id: lamp

		anchors.fill: parent
		radius: width

		color: {
			switch (connectionStateLamp.connectionState) {
			case (RedmineClient.OFFLINE):
				return "red"
			case (RedmineClient.TEMPORARY_OFFLINE):
				return "yellow"
			case (RedmineClient.ONLINE):
				return "green"
			}
		}

		MultiEffect {
			source: lamp
			anchors.fill: lamp
			shadowBlur: 0.1
			shadowEnabled: true
			shadowColor: "black"
			shadowVerticalOffset: 0
			shadowHorizontalOffset: 0
		}
	}
}
