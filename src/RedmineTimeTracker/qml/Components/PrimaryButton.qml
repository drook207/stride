import QtQuick
import QtQuick.Controls

/*!
 * \brief A view's main action button: a highlighted (accent-colored) button with the shared
 * control height, used instead of round buttons for text actions such as "Add".
 */
Button {
	highlighted: true
	implicitHeight: Metrics.controlHeight
	topInset: 0
	bottomInset: 0
	leftPadding: 24
	rightPadding: 24
}
