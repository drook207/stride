pragma Singleton
import QtQuick

/*!
 * \brief Parses and formats the decimal hours typed into the time fields.
 *
 * Always uses the "de_DE" locale (decimal comma, e.g. "1,5") regardless of the user's actual
 * system locale - matches the fields' "eg. 1,0" placeholder, but means "1.5" isn't accepted.
 */
QtObject {
	readonly property var locale: Qt.locale("de_DE")

	//! Hours in text, or NaN if it isn't a number.
	function parse(text) {
		try {
			return Number.fromLocaleString(locale, text)
		} catch (e) {
			return NaN
		}
	}

	//! hours as shown in the fields, with 2 decimals.
	function format(hours) {
		return Number(hours).toLocaleString(locale, 'f', 2)
	}
}
