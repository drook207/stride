import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QRedmine.TimeTracker

/*!
 * \brief Default/home view: lists locally tracked time entries (TimeTracker's local SQLite-backed
 * recentEntries model), newest first.
 */
Item {
	id: docroot

	required property TimeTracker timeTracker

	ColumnLayout {
		anchors {
			fill: parent
			margins: Metrics.pageMargin
		}
		spacing: Metrics.sectionSpacing

		PageTitle {
			Layout.fillWidth: true
			text: qsTr("Recent Entries")
		}

		Card {
			Layout.fillWidth: true
			Layout.fillHeight: true
			padding: 8

			ListView {
				id: recentEntries
				Layout.fillWidth: true
				Layout.fillHeight: true
				visible: count > 0
				model: docroot.timeTracker.recentEntries
				clip: true

				delegate: TimeEntryDelegate {
					required property var modelData
					required property int index

					issueId: modelData.issue_id
					hours: modelData.hours
					comment: modelData.comments || ""
					spentOn: modelData.spent_on
					showDivider: index < ListView.view.count - 1
				}

				ScrollBar.vertical: ScrollBar {}
			}

			// Empty state
			ColumnLayout {
				Layout.fillWidth: true
				Layout.fillHeight: true
				Layout.margins: 12
				visible: recentEntries.count === 0
				spacing: 6

				Item {
					Layout.fillHeight: true
				}
				Label {
					Layout.fillWidth: true
					text: qsTr("No time entries yet")
					color: Material.foreground
					font.pixelSize: 15
					font.weight: Font.DemiBold
					horizontalAlignment: Text.AlignHCenter
				}
				Label {
					Layout.fillWidth: true
					text: qsTr("Track time with the bar below or add an entry manually.")
					color: Material.secondaryTextColor
					horizontalAlignment: Text.AlignHCenter
					wrapMode: Text.WordWrap
				}
				Item {
					Layout.fillHeight: true
				}
			}
		}
	}
}
