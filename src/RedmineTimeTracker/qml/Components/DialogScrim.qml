import QtQuick

/*!
 * \brief Dark backdrop dimming the window behind a modal dialog.
 *
 * Used as `Overlay.modal: DialogScrim {}` by every modal dialog. Replaces the Material style's own
 * backdrop, which in the Dark theme is a 60% opaque near-white (Material.backgroundDimColor
 * #99FAFAFA) and turns the whole window light gray. A dark scrim, as in Material Design, keeps the
 * dimmed content readable and the dialog in focus.
 */
Rectangle {
	color: Qt.rgba(0, 0, 0, 0.6)

	// Same fade as the style's own backdrop.
	Behavior on opacity {
		NumberAnimation {
			duration: 150
		}
	}
}
