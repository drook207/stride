pragma Singleton
import QtQuick

/*!
 * \brief Shared sizes for a consistent layout across the whole UI.
 *
 * Only sizes live here - colors come from the Material style's attached properties, which a
 * singleton can't read with the window's theme applied.
 */
QtObject {
	//! Whether the UI runs on a touch platform (Android/iOS), mirroring Main.qml's isMobile.
	readonly property bool touch: Qt.platform.os === "android" || Qt.platform.os === "ios"

	//! Height of buttons, text fields and combo boxes. 44 on touch platforms (the usual minimum
	//! size for touch targets), 40 elsewhere.
	readonly property int controlHeight: touch ? 44 : 40
	//! Height of multi-line text areas.
	readonly property int textAreaHeight: 96
	//! Corner radius of surface panels (Sidebar, tracking bars, Cards).
	readonly property int panelRadius: 12
	//! Outer margin around a view's content. The desktop shell already spaces the views from the
	//! sidebar and window edges; the mobile shell doesn't.
	readonly property int pageMargin: touch ? 16 : 0
	//! Spacing between a page's sections (e.g. its title and Cards).
	readonly property int sectionSpacing: 16
	//! Maximum width of form content, so fields don't stretch across very wide windows.
	readonly property int formMaxWidth: 720
}
