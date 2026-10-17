import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Controls.impl
import QtQuick.Layouts

/*!
 * \brief One time entry in a list: issue number, comment, date and the duration as an accent pill.
 *
 * Shared by RecentEntriesView (local entries) and GetTimeEntriesView (entries fetched from
 * Redmine), so both lists look the same. Set the properties from the respective model's fields.
 */
ItemDelegate {
	id: delegate

	required property int issueId
	//! Duration in decimal hours (e.g. 1.5).
	required property real hours
	required property string comment
	//! Date the time was spent on, as Date or date string - whatever the model provides.
	required property var spentOn
	//! Redmine's id of the time entry itself; hidden when negative (e.g. not synced yet).
	property int entryId: -1
	//! Draws a divider below this entry; disable it for the last one.
	property bool showDivider: true
	//! Whether clicking the entry edits it (the view handles `clicked`); shows a pen icon on hover.
	property bool editable: false

	//! Formats a decimal hour count (e.g. 1.5) as "HH:MM:SS" (e.g. "01:30:00").
	function formatDuration(hours) {
		const totalSeconds = Math.round(hours * 3600)
		const h = Math.floor(totalSeconds / 3600)
		const m = Math.floor((totalSeconds % 3600) / 60)
		const s = totalSeconds % 60
		function pad(value) {
			return value < 10 ? "0" + value : "" + value
		}
		return pad(h) + ":" + pad(m) + ":" + pad(s)
	}

	width: ListView.view ? ListView.view.width : implicitWidth

	HoverHandler {
		cursorShape: delegate.editable ? Qt.PointingHandCursor : Qt.ArrowCursor
	}
	topPadding: 12
	bottomPadding: 12
	leftPadding: 12
	rightPadding: 12

	background: Rectangle {
		radius: 8
		color: delegate.hovered ? Qt.rgba(1, 1, 1, 0.04) : "transparent"

		Rectangle {
			visible: delegate.showDivider
			anchors {
				left: parent.left
				right: parent.right
				bottom: parent.bottom
				leftMargin: 12
				rightMargin: 12
			}
			height: 1
			color: Material.dividerColor
		}
	}

	contentItem: RowLayout {
		spacing: 14

		// Icon in an accent-tinted circle, like the sidebar's active item.
		Rectangle {
			Layout.alignment: Qt.AlignTop
			implicitWidth: 36
			implicitHeight: 36
			radius: 18
			color: Qt.rgba(Material.accentColor.r, Material.accentColor.g, Material.accentColor.b, 0.24)

			IconImage {
				anchors.centerIn: parent
				source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/clock-rotate-left.svg"
				sourceSize.width: 16
				sourceSize.height: 16
				color: "#E8E1F5"
			}
		}

		ColumnLayout {
			Layout.fillWidth: true
			spacing: 2

			RowLayout {
				Layout.fillWidth: true
				spacing: 8

				Label {
					text: "#" + delegate.issueId
					color: Material.foreground
					font.pixelSize: 15
					font.weight: Font.DemiBold
				}
				Label {
					visible: delegate.entryId >= 0
					text: qsTr("Entry %1").arg(delegate.entryId)
					color: Material.hintTextColor
					font.pixelSize: 12
				}
				Item {
					Layout.fillWidth: true
				}
				Label {
					text: Qt.formatDate(delegate.spentOn, "dd.MM.yy")
					color: Material.hintTextColor
					font.pixelSize: 12
				}
			}

			Label {
				Layout.fillWidth: true
				visible: delegate.comment.length > 0
				text: delegate.comment
				color: Material.secondaryTextColor
				wrapMode: Text.WordWrap
			}
		}

		IconImage {
			Layout.alignment: Qt.AlignVCenter
			visible: delegate.editable
			opacity: delegate.hovered || delegate.visualFocus ? 1 : 0
			source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/pen.svg"
			sourceSize.width: 14
			sourceSize.height: 14
			color: Material.secondaryTextColor

			Behavior on opacity {
				NumberAnimation {
					duration: 100
				}
			}
		}

		Rectangle {
			Layout.alignment: Qt.AlignVCenter
			implicitWidth: durationLabel.implicitWidth + 20
			implicitHeight: 26
			radius: 13
			color: Qt.rgba(Material.accentColor.r, Material.accentColor.g, Material.accentColor.b, 0.24)

			Label {
				id: durationLabel
				anchors.centerIn: parent
				text: delegate.formatDuration(delegate.hours)
				color: "#E8E1F5"
				font.pixelSize: 13
				font.weight: Font.DemiBold
			}
		}
	}
}
