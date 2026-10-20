pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Controls.impl
import QtQuick.Layouts
import QRedmine.TimeTracker

/*!
 * \brief Time-of-day input: a field showing the selected time that opens hour/minute wheels.
 *
 * Starts empty (`hasTime` false); picking a time sets `hours`/`minutes` and emits timePicked(),
 * the small clear button empties it again. Shown in the UI language selected in the settings
 * (LanguageManager): 24-hour clock where that locale uses one ("13:00"), otherwise 12-hour with
 * an AM/PM wheel ("1:00 PM"). Minutes are picked in 5-minute steps; the wheels
 * preselect the step at or before the shown time, never a later one.
 */
TextField {
	id: field

	//! Whether a time has been picked; hours/minutes are meaningless otherwise.
	property bool hasTime: false
	//! Picked hour, 0-23.
	property int hours: 9
	//! Picked minute, 0-59.
	property int minutes: 0
	//! Picked time as minutes since midnight.
	readonly property int totalMinutes: field.hours * 60 + field.minutes
	//! Time the wheels start at while no time is picked, as minutes since midnight; 9:00 if
	//! negative.
	property int initialMinutes: -1
	//! Whether the wheels can be opened. Unlike `enabled`, the field still receives hover, so it can
	//! explain with a tooltip why it is locked.
	property bool pickable: true

	//! Emitted when a time is picked (not when it's cleared).
	signal timePicked

	//! Locale of the UI language, or the system locale when "System default" is selected.
	readonly property var uiLocale: LanguageManager.language.length > 0 ? Qt.locale(LanguageManager.language)
																		 : Qt.locale()
	//! Whether the locale writes times with AM/PM.
	readonly property bool twelveHour: field.uiLocale.timeFormat(Locale.ShortFormat).toLowerCase().indexOf("ap") >= 0

	function open() {
		if (!field.pickable) {
			return
		}
		// Preselect the picked time, or initialMinutes (9:00 by default) when there is none.
		const start = field.hasTime ? field.totalMinutes : field.initialMinutes >= 0 ? field.initialMinutes % (24 * 60) : 9 * 60
		const hour = Math.floor(start / 60)
		const hour12 = hour % 12 === 0 ? 12 : hour % 12
		hourTumbler.currentIndex = field.twelveHour ? hour12 - 1 : hour
		minuteTumbler.currentIndex = Math.floor((start % 60) / 5)
		amPmTumbler.currentIndex = hour >= 12 ? 1 : 0
		popup.open()
	}

	//! Applies the wheels' selection.
	function accept() {
		let hour = field.twelveHour ? hourTumbler.currentIndex + 1 : hourTumbler.currentIndex
		if (field.twelveHour) {
			hour = hour % 12 + (amPmTumbler.currentIndex === 1 ? 12 : 0)
		}
		field.hours = hour
		field.minutes = minuteTumbler.currentIndex * 5
		field.hasTime = true
		popup.close()
		field.timePicked()
	}

	readOnly: true
	// "--:--" as dimmed text rather than placeholderText, which the Material style floats into the
	// outline (over the LabeledField's label) once the field has focus or text.
	text: field.hasTime ? new Date(2000, 0, 1, field.hours, field.minutes).toLocaleTimeString(field.uiLocale, Locale.ShortFormat) : "--:--"
	color: field.hasTime ? Material.foreground : Material.hintTextColor
	rightPadding: clockIcon.width + clearButton.width + 28

	// Picking is the only way to change the value, so the whole field acts as a button; see
	// DatePickerField for why this is a MouseArea rather than a TapHandler.
	MouseArea {
		anchors.fill: parent
		cursorShape: Qt.PointingHandCursor
		onClicked: {
			field.forceActiveFocus()
			field.open()
		}
	}
	Keys.onSpacePressed: field.open()
	Keys.onReturnPressed: field.open()
	Keys.onEnterPressed: field.open()

	ClearButton {
		id: clearButton
		anchors {
			right: clockIcon.left
			rightMargin: 4
			verticalCenter: parent.verticalCenter
		}
		visible: field.hasTime
		onClicked: field.hasTime = false
	}

	IconImage {
		id: clockIcon
		anchors {
			right: parent.right
			rightMargin: 14
			verticalCenter: parent.verticalCenter
		}
		source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/clock.svg"
		sourceSize.width: 16
		sourceSize.height: 16
		color: Material.secondaryTextColor
	}

	component Wheel: Tumbler {
		implicitWidth: 56
		implicitHeight: 180
		visibleItemCount: 5

		delegate: Label {
			required property var modelData
			required property int index

			text: modelData
			font.pixelSize: 16
			font.weight: Tumbler.displacement === 0 ? Font.DemiBold : Font.Normal
			color: Tumbler.displacement === 0 ? Material.foreground : Material.secondaryTextColor
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
			opacity: 1.0 - Math.abs(Tumbler.displacement) / (Tumbler.tumbler.visibleItemCount / 2)
		}
	}

	Popup {
		id: popup

		y: field.height + 4
		padding: 12
		modal: false
		focus: true

		// Same look as DatePickerField's calendar.
		background: Rectangle {
			radius: Metrics.panelRadius
			color: Qt.lighter(Material.dialogColor, 1.25)
			border.width: 1
			border.color: Qt.rgba(1, 1, 1, 0.08)
		}

		contentItem: ColumnLayout {
			spacing: 8
			// Enter confirms the selection (Escape already cancels), so the picker works from the
			// keyboard without tabbing to the OK button.
			focus: true
			Keys.onReturnPressed: field.accept()
			Keys.onEnterPressed: field.accept()

			RowLayout {
				Layout.alignment: Qt.AlignHCenter
				spacing: 4

				Item {
					implicitWidth: wheels.implicitWidth
					implicitHeight: wheels.implicitHeight

					// Highlight band behind the selected row of all wheels.
					Rectangle {
						anchors.verticalCenter: parent.verticalCenter
						width: parent.width
						height: 36
						radius: 18
						color: Qt.rgba(Material.accentColor.r, Material.accentColor.g, Material.accentColor.b, 0.24)
					}

					RowLayout {
						id: wheels
						spacing: 0

						Wheel {
							id: hourTumbler
							model: field.twelveHour ? 12 : 24
							delegate: Label {
								required property int index
								text: field.twelveHour ? index + 1 : (index < 10 ? "0" + index : index)
								font.pixelSize: 16
								font.weight: Tumbler.displacement === 0 ? Font.DemiBold : Font.Normal
								color: Tumbler.displacement === 0 ? Material.foreground : Material.secondaryTextColor
								horizontalAlignment: Text.AlignHCenter
								verticalAlignment: Text.AlignVCenter
								opacity: 1.0 - Math.abs(Tumbler.displacement) / (Tumbler.tumbler.visibleItemCount / 2)
							}
						}
						Label {
							text: ":"
							font.pixelSize: 16
							font.weight: Font.DemiBold
							color: Material.foreground
						}
						Wheel {
							id: minuteTumbler
							model: ["00", "05", "10", "15", "20", "25", "30", "35", "40", "45", "50", "55"]
						}
						Wheel {
							id: amPmTumbler
							visible: field.twelveHour
							wrap: false
							model: [field.uiLocale.amText, field.uiLocale.pmText]
						}
					}
				}
			}

			RowLayout {
				Layout.alignment: Qt.AlignRight
				spacing: 4

				Button {
					flat: true
					text: qsTr("Cancel")
					onClicked: popup.close()
				}
				Button {
					flat: true
					highlighted: true
					text: qsTr("OK")
					onClicked: field.accept()
				}
			}
		}
	}
}
