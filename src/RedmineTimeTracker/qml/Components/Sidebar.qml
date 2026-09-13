import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QRedmine.TimeTracker

Rectangle {
	id: sidebar

	required property StackView view
	required property var componentsToLoad
	required property var buttonTitles
	required property TimeTracker timeTracker

	property int currentIndex: 0
	property bool expanded: true

	width: expanded ? 220 : 0
	clip: true
	color: Material.dialogColor

	Behavior on width {
		NumberAnimation {
			duration: 150
		}
	}

	function select(index) {
		sidebar.currentIndex = index
		sidebar.view.replaceCurrentItem(sidebar.componentsToLoad[index])
	}

	readonly property var navIcons: ["clock-rotate-left.svg", "circle-plus.svg", "magnifying-glass.svg", "circle-plus.svg"]

	readonly property color activePillColor: "#E8E1F5"
	readonly property color activeForegroundColor: "#1A1A1A"

	ColumnLayout {
		anchors {
			left: parent.left
			right: parent.right
			top: parent.top
			margins: 15
		}
		spacing: 5

		Label {
			text: qsTr("Menu")
			color: Material.hintTextColor
			font.pixelSize: 12
			Layout.bottomMargin: 10
		}

		Repeater {
			model: sidebar.buttonTitles

			ItemDelegate {
				id: navItem
				required property int index

				Layout.fillWidth: true
				Layout.preferredHeight: 40

				text: sidebar.buttonTitles[index]
				icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/" + sidebar.navIcons[index]
				icon.color: navItem.checked ? sidebar.activeForegroundColor : Material.foreground
				checked: sidebar.currentIndex === index
				display: AbstractButton.TextBesideIcon

				background: Rectangle {
					radius: 20
					color: navItem.checked ? sidebar.activePillColor : "transparent"
				}

				contentItem: RowLayout {
					spacing: 10
					Image {
						source: navItem.icon.source
						sourceSize.width: 16
						sourceSize.height: 16
					}
					Label {
						text: navItem.text
						color: navItem.icon.color
						Layout.fillWidth: true
					}
					Rectangle {
						visible: navItem.index === 0
						radius: height / 2
						color: Material.accentColor
						implicitWidth: countLabel.implicitWidth + 12
						implicitHeight: 20

						Label {
							id: countLabel
							anchors.centerIn: parent
							text: sidebar.timeTracker.recentEntries.length
							color: Material.primaryHighlightedTextColor
							font.pixelSize: 11
						}
					}
				}

				onClicked: sidebar.select(index)
			}
		}

		Rectangle {
			Layout.fillWidth: true
			Layout.topMargin: 10
			Layout.bottomMargin: 10
			Layout.preferredHeight: 1
			color: Material.dividerColor
		}

		ItemDelegate {
			id: settingsItem

			Layout.fillWidth: true
			Layout.preferredHeight: 40

			text: qsTr("Settings")
			icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/gear.svg"
			icon.color: settingsItem.checked ? sidebar.activeForegroundColor : Material.foreground
			checked: sidebar.currentIndex === sidebar.buttonTitles.length
			display: AbstractButton.TextBesideIcon

			background: Rectangle {
				radius: 20
				color: settingsItem.checked ? sidebar.activePillColor : "transparent"
			}

			contentItem: RowLayout {
				spacing: 10
				Image {
					source: settingsItem.icon.source
					sourceSize.width: 16
					sourceSize.height: 16
				}
				Label {
					text: settingsItem.text
					color: settingsItem.icon.color
					Layout.fillWidth: true
				}
			}

			onClicked: sidebar.select(sidebar.buttonTitles.length)
		}
	}
}
