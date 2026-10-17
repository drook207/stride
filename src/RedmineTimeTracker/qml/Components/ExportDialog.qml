import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief Exports the time entries of a period as CSV or PDF (see TimeEntryExport).
 *
 * Asks for the period and format, then for the file. The entries come from Redmine (plus the
 * local ones not sent yet); while it can't be reached, only the entries known in the app are
 * exported, which the result message says.
 */
Dialog {
	id: dialog

	required property RedmineClient client

	//! Opens the dialog for a period, e.g. the week shown in the overview.
	function openFor(from, to) {
		fromField.selectedDate = from
		toField.selectedDate = to
		dialog.open()
	}

	//! Emitted once a file was written (or writing it failed).
	signal finished()

	//! The file chosen, while waiting for Redmine's entries.
	property url pendingFile
	property bool waiting: false

	function finish(remoteEntries, includesRemote) {
		dialog.waiting = false
		fallbackTimer.stop()
		const rows = exporter.rows(remoteEntries, includesRemote, fromField.selectedDate, toField.selectedDate)
		const title = qsTr("Time entries %1 – %2").arg(fromField.selectedDate.toLocaleDateString(Qt.locale(), Locale.ShortFormat))
		.arg(toField.selectedDate.toLocaleDateString(Qt.locale(), Locale.ShortFormat))
		const written = formatCombo.currentIndex === 0 ? exporter.writeCsv(dialog.pendingFile, rows)
													   : exporter.writePdf(dialog.pendingFile, rows, title)
		resultDialog.text = !written ? qsTr("The file couldn't be written: %1").arg(exporter.lastError)
									 : includesRemote ? qsTr("Exported %n time entries.", "", rows.length)
													  : qsTr("Exported %n time entries. Redmine couldn't be reached, so only the entries known in the app are included.", "", rows.length)
		resultDialog.open()
		dialog.finished()
	}

	parent: Overlay.overlay
	anchors.centerIn: parent
	width: Math.min(parent.width - 32, 480)
	modal: true
	Overlay.modal: DialogScrim {}
	title: qsTr("Export time entries")
	standardButtons: Dialog.Ok | Dialog.Cancel

	Component.onCompleted: standardButton(Dialog.Ok).text = qsTr("Export…")
	onAccepted: {
		fileDialog.selectedFile = "file:///time-entries-" + fromField.isoDate + "-" + toField.isoDate
								  + (formatCombo.currentIndex === 0 ? ".csv" : ".pdf")
		fileDialog.open()
	}

	TimeEntryExport {
		id: exporter
	}

	FileDialog {
		id: fileDialog
		fileMode: FileDialog.SaveFile
		nameFilters: formatCombo.currentIndex === 0 ? [qsTr("CSV files (*.csv)")] : [qsTr("PDF files (*.pdf)")]
		defaultSuffix: formatCombo.currentIndex === 0 ? "csv" : "pdf"
		onAccepted: {
			dialog.pendingFile = fileDialog.selectedFile
			if (dialog.client.connectionState === RedmineClient.ONLINE) {
				dialog.waiting = true
				fallbackTimer.restart()
				dialog.client.getTimeEntryReport(fromField.isoDate, toField.isoDate)
			} else {
				dialog.finish([], false)
			}
		}
	}

	Connections {
		target: dialog.client
		enabled: dialog.waiting
		function onTimeEntryReportReceived(timeEntries, from, to) {
			if (from === fromField.isoDate && to === toField.isoDate) {
				dialog.finish(timeEntries, true)
			}
		}
	}

	// Redmine didn't answer in time (the request fails quietly): export what's known.
	Timer {
		id: fallbackTimer
		interval: 15000
		onTriggered: {
			if (dialog.waiting) {
				dialog.finish([], false)
			}
		}
	}

	MessageDialog {
		id: resultDialog
	}

	contentItem: ColumnLayout {
		spacing: 12

		RowLayout {
			Layout.fillWidth: true
			spacing: 12

			LabeledField {
				Layout.fillWidth: true
				label: qsTr("From")
				DatePickerField {
					id: fromField
					Layout.fillWidth: true
					Layout.preferredHeight: Metrics.controlHeight
				}
			}
			LabeledField {
				Layout.fillWidth: true
				label: qsTr("To")
				DatePickerField {
					id: toField
					Layout.fillWidth: true
					Layout.preferredHeight: Metrics.controlHeight
				}
			}
		}

		LabeledField {
			Layout.fillWidth: true
			label: qsTr("Format")
			ComboBox {
				id: formatCombo
				Layout.fillWidth: true
				Layout.preferredHeight: Metrics.controlHeight
				topInset: 0
				bottomInset: 0
				model: [qsTr("CSV (spreadsheets)"), qsTr("PDF")]
			}
		}
	}
}
