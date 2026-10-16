import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
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
	// Hidden once fully collapsed, so keyboard focus (Tab) never lands on an invisible nav item.
	visible: width > 0
	// Its own surface (the same as the TrackingBar's) so the sidebar reads as a panel rather than
	// blending into the content next to it.
	color: Material.dialogColor
	radius: Metrics.panelRadius

	Behavior on width {
		NumberAnimation {
			duration: 150
		}
	}

	//! Selects the item at `index` in the StackView (updates the highlighted item and swaps its
	//! current item). The Settings item below calls this with buttonTitles.length.
	function select(index) {
		sidebar.currentIndex = index
		sidebar.view.replaceCurrentItem(sidebar.componentsToLoad[index])
	}

	//! Icon filename per main nav item (under Assets/Icons/solid/), index-aligned with
	//! componentsToLoad and buttonTitles. Must be kept the same length as those two.
	readonly property var navIcons: ["clock-rotate-left.svg", "circle-plus.svg", "magnifying-glass.svg", "circle-plus.svg"]

	//! Active item: an accent-tinted pill with light lavender icon and text (the Material Design
	//! navigation style); inactive items use the secondary text color.
	readonly property color activePillColor: Qt.rgba(Material.accentColor.r, Material.accentColor.g, Material.accentColor.b, 0.24)
	readonly property color activeForegroundColor: "#E8E1F5"
	readonly property color inactiveForegroundColor: Material.secondaryTextColor

	//! One navigation entry. Shared by the main entries and Settings so both always look and
	//! behave the same.
	component NavItem: ItemDelegate {
		id: navItem

		//! Index passed to sidebar.select() and compared with sidebar.currentIndex.
		required property int navIndex
		//! Icon filename under Assets/Icons/solid/.
		required property string iconName
		//! Count shown in a badge after the label; hidden when negative.
		property int badgeCount: -1

		readonly property color foregroundColor: navItem.checked ? sidebar.activeForegroundColor : sidebar.inactiveForegroundColor

		Layout.fillWidth: true
		Layout.preferredHeight: 40

		icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/" + navItem.iconName
		checked: sidebar.currentIndex === navItem.navIndex
		// ItemDelegate isn't focusable by default; make the nav reachable with Tab. Space activates
		// it like any button, Enter is handled below.
		focusPolicy: Qt.StrongFocus
		leftPadding: 16
		rightPadding: 12

		background: Rectangle {
			radius: 20
			color: navItem.checked ? sidebar.activePillColor
								   : navItem.down ? Qt.rgba(1, 1, 1, 0.10)
												  : navItem.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
			// Focus ring, only for keyboard focus (the custom background hides the style's own).
			border.width: navItem.visualFocus ? 2 : 0
			border.color: Material.accentColor
		}

		contentItem: RowLayout {
			spacing: 12

			// IconImage rather than Image: only it applies a color to the (black) SVG icons.
			IconImage {
				source: navItem.icon.source
				sourceSize.width: 16
				sourceSize.height: 16
				color: navItem.foregroundColor
			}
			Label {
				Layout.fillWidth: true
				text: navItem.text
				color: navItem.foregroundColor
				font.weight: navItem.checked ? Font.DemiBold : Font.Normal
				elide: Text.ElideRight
			}
			Rectangle {
				visible: navItem.badgeCount >= 0
				radius: height / 2
				color: Material.accentColor
				implicitWidth: Math.max(implicitHeight, countLabel.implicitWidth + 12)
				implicitHeight: 20

				Label {
					id: countLabel
					anchors.centerIn: parent
					text: navItem.badgeCount
					color: Material.backgroundColor
					font.pixelSize: 11
					font.bold: true
				}
			}
		}

		onClicked: sidebar.select(navItem.navIndex)
		Keys.onReturnPressed: sidebar.select(navItem.navIndex)
		Keys.onEnterPressed: sidebar.select(navItem.navIndex)
	}

	ColumnLayout {
		anchors {
			left: parent.left
			right: parent.right
			top: parent.top
			margins: 12
		}
		spacing: 4

		Label {
			Layout.leftMargin: 16
			Layout.topMargin: 4
			Layout.bottomMargin: 8
			text: qsTr("Menu")
			color: Material.secondaryTextColor
			font.pixelSize: 12
		}

		Repeater {
			model: sidebar.buttonTitles

			NavItem {
				required property int index

				navIndex: index
				iconName: sidebar.navIcons[index]
				text: sidebar.buttonTitles[index]
				badgeCount: index === 0 ? sidebar.timeTracker.recentEntries.length : -1
			}
		}

		Rectangle {
			Layout.fillWidth: true
			Layout.topMargin: 8
			Layout.bottomMargin: 8
			Layout.preferredHeight: 1
			color: Material.dividerColor
		}

		NavItem {
			navIndex: sidebar.buttonTitles.length
			iconName: "gear.svg"
			text: qsTr("Settings")
		}
	}
}
