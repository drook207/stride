import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Controls.impl
import QtQuick.Layouts
import QRedmine.TimeTracker

/*!
 * \brief Date input: a field showing the selected date that opens a month calendar to pick one.
 *
 * Used instead of a free-text "yyyy-mm-dd" field. Read `isoDate` for the value Redmine expects.
 * The date is shown in the UI language selected in the settings (LanguageManager), and the
 * calendar starts its weeks on that locale's first day of the week.
 */
TextField {
	id: field

	//! The selected date (local time, time of day ignored). Defaults to today.
	property date selectedDate: new Date()
	//! selectedDate as "yyyy-MM-dd", the format Redmine's spent_on uses.
	readonly property string isoDate: Qt.formatDate(field.selectedDate, "yyyy-MM-dd")

	//! Locale of the UI language, or the system locale when "System default" is selected.
	readonly property var uiLocale: LanguageManager.language.length > 0 ? Qt.locale(LanguageManager.language)
																		 : Qt.locale()

	function open() {
		calendar.month = field.selectedDate.getMonth()
		calendar.year = field.selectedDate.getFullYear()
		popup.open()
	}

	function pickDate(date) {
		field.selectedDate = date
		popup.close()
	}

	readOnly: true
	text: field.selectedDate.toLocaleDateString(field.uiLocale, Locale.LongFormat)
	rightPadding: calendarIcon.width + 24

	// Picking from the calendar is the only way to change the value, so the whole field acts as a
	// button. A MouseArea rather than a TapHandler: the TextField accepts presses itself (to focus
	// and select text), which cancels a TapHandler's tap whenever another field had the focus.
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

	IconImage {
		id: calendarIcon
		anchors {
			right: parent.right
			rightMargin: 14
			verticalCenter: parent.verticalCenter
		}
		source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/calendar-days.svg"
		sourceSize.width: 16
		sourceSize.height: 16
		color: Material.secondaryTextColor
	}

	Popup {
		id: popup

		y: field.height + 4
		width: 300
		padding: 12
		modal: false
		focus: true

		// Slightly lighter than the Card it usually opens over, so it reads as a layer above it.
		background: Rectangle {
			radius: Metrics.panelRadius
			color: Qt.lighter(Material.dialogColor, 1.25)
			border.width: 1
			border.color: Qt.rgba(1, 1, 1, 0.08)
		}

		contentItem: ColumnLayout {
			spacing: 8

			// Month navigation
			RowLayout {
				Layout.fillWidth: true

				ToolButton {
					icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/chevron-left.svg"
					icon.width: 12
					icon.height: 12
					onClicked: {
						if (calendar.month === 0) {
							calendar.month = 11
							calendar.year -= 1
						} else {
							calendar.month -= 1
						}
					}
				}
				Label {
					Layout.fillWidth: true
					horizontalAlignment: Text.AlignHCenter
					text: field.uiLocale.standaloneMonthName(calendar.month) + " " + calendar.year
					color: Material.foreground
					font.weight: Font.DemiBold
				}
				ToolButton {
					icon.source: "qrc:/qt/qml/QRedmine/TimeTracker/qml/Assets/Icons/solid/chevron-right.svg"
					icon.width: 12
					icon.height: 12
					onClicked: {
						if (calendar.month === 11) {
							calendar.month = 0
							calendar.year += 1
						} else {
							calendar.month += 1
						}
					}
				}
			}

			DayOfWeekRow {
				Layout.fillWidth: true
				locale: calendar.locale

				delegate: Label {
					required property string shortName
					text: shortName
					color: Material.hintTextColor
					font.pixelSize: 12
					horizontalAlignment: Text.AlignHCenter
				}
			}

			MonthGrid {
				id: calendar
				Layout.fillWidth: true
				locale: field.uiLocale
				spacing: 2

				delegate: Item {
					id: dayItem

					required property int day
					required property int month
					required property int year
					required property bool today

					// Built from the components rather than the delegate's `date`, which is midnight
					// UTC and can show as the previous day west of UTC.
					readonly property date localDate: new Date(dayItem.year, dayItem.month, dayItem.day)
					readonly property bool selected: dayItem.year === field.selectedDate.getFullYear()
													&& dayItem.month === field.selectedDate.getMonth()
													&& dayItem.day === field.selectedDate.getDate()
					readonly property bool inMonth: dayItem.month === calendar.month

					implicitWidth: 36
					implicitHeight: 36

					Rectangle {
						anchors.centerIn: parent
						width: 34
						height: 34
						radius: 17
						color: dayItem.selected ? Material.accentColor
												: dayHover.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
						border.width: dayItem.today && !dayItem.selected ? 1 : 0
						border.color: Material.accentColor
					}
					Label {
						anchors.centerIn: parent
						text: dayItem.day
						font.pixelSize: 13
						font.weight: dayItem.selected ? Font.DemiBold : Font.Normal
						color: dayItem.selected ? Material.backgroundColor
												: dayItem.inMonth ? Material.foreground : Material.hintTextColor
					}

					HoverHandler {
						id: dayHover
					}
					TapHandler {
						onTapped: field.pickDate(dayItem.localDate)
					}
				}
			}

			Button {
				Layout.alignment: Qt.AlignRight
				flat: true
				highlighted: true
				text: qsTr("Today")
				onClicked: field.pickDate(new Date())
			}
		}
	}
}
