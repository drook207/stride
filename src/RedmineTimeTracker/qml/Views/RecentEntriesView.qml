import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Effects
import QRedmine.TimeTracker

/*!
 * \brief Default/home view: lists locally tracked time entries (TimeTracker's local SQLite-backed
 * recentEntries model), newest first.
 */
Item {
	id: docroot

	required property TimeTracker timeTracker

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

	ListView {
		id: recentEntries
		anchors.fill: parent
		model: docroot.timeTracker.recentEntries
		clip: true

		delegate: ItemDelegate {
			required property var modelData

			width: ListView.view.width
			height: rowLayout.implicitHeight + 20

			contentItem: RowLayout {
				id: rowLayout
				anchors.verticalCenter: parent.verticalCenter
				spacing: 15

				Image {
					id: personIcon
					source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/user.svg"
					sourceSize.width: 24
					sourceSize.height: 24
					visible: false
				}

				MultiEffect {
					source: personIcon
					Layout.preferredWidth: personIcon.width
					Layout.preferredHeight: personIcon.height
					colorization: 1.0
					colorizationColor: Material.foreground
				}

				ColumnLayout {
					Layout.fillWidth: true
					spacing: 2

					Label {
						text: docroot.formatDuration(modelData.hours)
						color: Material.hintTextColor
						font.pixelSize: 12
					}

					Label {
						text: "#" + modelData.issue_id
						font.bold: true
						font.pixelSize: 16
					}

					Label {
						text: modelData.comments
						visible: modelData.comments !== ""
						color: Material.secondaryTextColor
						wrapMode: Text.WordWrap
						Layout.fillWidth: true
					}
				}

				Label {
					text: Qt.formatDate(modelData.spent_on, "dd.MM.yy")
					color: Material.hintTextColor
					font.pixelSize: 12
					Layout.alignment: Qt.AlignTop
				}
			}

			Rectangle {
				anchors {
					left: parent.left
					right: parent.right
					bottom: parent.bottom
				}
				height: 1
				color: Material.dividerColor
			}
		}

		ScrollBar.vertical: ScrollBar {}
	}
}
