import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
 * \brief Rounded surface panel grouping related content, in the same style as the Sidebar and
 * the tracking bars.
 *
 * Children are stacked in a ColumnLayout, so they use Layout attached properties, e.g.
 * `Card { title: qsTr("..."); LabeledField { Layout.fillWidth: true; ... } }`. A child with
 * Layout.fillHeight (e.g. a ListView) fills the card when the card itself is given a height.
 */
Rectangle {
	id: card

	//! Optional heading shown at the top of the card.
	property string title: ""
	//! Space between the card's edge and its content.
	property int padding: 20
	default property alias content: column.data

	color: Material.dialogColor
	radius: Metrics.panelRadius
	implicitWidth: column.implicitWidth + 2 * card.padding
	implicitHeight: column.implicitHeight + 2 * card.padding

	ColumnLayout {
		id: column
		anchors {
			fill: parent
			margins: card.padding
		}
		spacing: 12

		Label {
			visible: card.title.length > 0
			text: card.title
			color: Material.foreground
			font.pixelSize: 15
			font.weight: Font.DemiBold
		}
	}
}
