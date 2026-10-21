import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/*!
 * \brief Base layout for form-like views: a PageTitle above vertically stacked content (typically
 * Cards), limited to Metrics.formMaxWidth and scrolling when it's taller than the view.
 *
 * Usage: `FormPage { title: qsTr("..."); Card { Layout.fillWidth: true; ... } }`. Views that
 * mainly show a list scroll the list itself instead and don't use this.
 */
Item {
	id: page

	property alias title: pageTitle.text
	default property alias content: column.data

	Flickable {
		id: flickable
		anchors.fill: parent
		contentWidth: width
		contentHeight: column.implicitHeight + 2 * Metrics.pageMargin
		boundsBehavior: Flickable.StopAtBounds
		clip: true

		ScrollBar.vertical: ScrollBar {}

		ColumnLayout {
			id: column
			x: Metrics.pageMargin
			y: Metrics.pageMargin
			width: Math.min(flickable.width - 2 * Metrics.pageMargin, Metrics.formMaxWidth)
			spacing: Metrics.sectionSpacing

			PageTitle {
				id: pageTitle
				Layout.fillWidth: true
			}
		}
	}
}
