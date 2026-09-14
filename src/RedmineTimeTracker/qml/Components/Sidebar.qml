import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QRedmine.TimeTracker

/*!
 * \brief Desktop-only collapsible navigation sidebar (replaced on mobile by MobileTabBar).
 *
 * "Settings" is rendered as a separate, always-present item below the main nav list rather than
 * a regular Repeater entry, but it still lives at componentsToLoad[buttonTitles.length] - i.e.
 * callers pass one more entry in componentsToLoad than in buttonTitles/navIcons, with that extra
 * trailing entry being the Settings view. See DesktopShell for how these three arrays are built.
 */
Rectangle {
	id: sidebar

	//! StackView whose current item this sidebar swaps via select().
	required property StackView view
	//! Component per nav item, index-aligned with buttonTitles/navIcons - plus one extra trailing
	//! entry (at index buttonTitles.length) for the Settings view, see the type-level comment.
	required property var componentsToLoad
	//! Label per main nav item (Settings excluded - its label is hardcoded below), index-aligned
	//! with navIcons and with componentsToLoad's first buttonTitles.length entries.
	required property var buttonTitles
	required property TimeTracker timeTracker

	property int currentIndex: 0
	//! Whether the sidebar is showing (width 220) or fully collapsed (width 0). Toggled by
	//! Header's hamburger button via DesktopShell.
	property bool expanded: true

	width: expanded ? 220 : 0
	clip: true
	color: "transparent"

	Behavior on width {
		NumberAnimation {
			duration: 150
		}
	}

	//! Selects the item at `index` in the StackView (updates the highlighted item and swaps its
	//! current item). Settings's onClicked handler below calls this with buttonTitles.length.
	function select(index) {
		sidebar.currentIndex = index
		sidebar.view.replaceCurrentItem(sidebar.componentsToLoad[index])
	}

	//! Icon filename per main nav item (under Assets/Icons/solid/), index-aligned with
	//! componentsToLoad and buttonTitles. Must be kept the same length as those two.
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
