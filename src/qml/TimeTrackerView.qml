// TimeTrackerView.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import drook207.timeTracker

Item {

	id: docroot
	property TimeTracker timeTracker: TimeTracker {}
	property RedmineClient client: undefined
	ColumnLayout {
		anchors.fill: parent
		spacing: 15

		GroupBox {
			title: "Time Tracking"
			Layout.fillWidth: true

			ColumnLayout {
				anchors.fill: parent
				spacing: 10

				GridLayout {
					columns: 2
					Layout.fillWidth: true
					rowSpacing: 10
					columnSpacing: 10

					Label {
						text: "Issue ID:"
					}
					TextField {
						id: issueIdField
						Layout.fillWidth: true
						placeholderText: "Enter issue ID"
						validator: IntValidator {
							bottom: 1
						}
						enabled: !docroot.timeTracker.isTracking
					}

					Label {
						text: "Elapsed Time:"
					}
					Label {
						text: docroot.timeTracker.elapsedTime
						font.pixelSize: 24
						font.bold: true
						color: docroot.timeTracker.isPaused ? "#FFA500" : "#008000"
					}
				}

				TextArea {
					id: commentsField
					Layout.fillWidth: true
					Layout.preferredHeight: 80
					placeholderText: "Enter comments for this time entry"
					wrapMode: TextEdit.Wrap
				}

				RowLayout {
					Layout.fillWidth: true
					spacing: 10

					Button {
						text: !docroot.timeTracker.isTracking ? "Start Tracking" : docroot.timeTracker.isPaused ? "Resume" : "Pause"
						enabled: issueIdField.text !== ""
								 || docroot.timeTracker.isTracking
						highlighted: docroot.timeTracker.isTracking
									 && !docroot.timeTracker.isPaused
						onClicked: {
							if (!docroot.timeTracker.isTracking) {
								docroot.timeTracker.startTracking(
											parseInt(issueIdField.text))
							} else if (docroot.timeTracker.isPaused) {
								docroot.timeTracker.resumeTracking()
							} else {
								docroot.timeTracker.pauseTracking()
							}
						}
					}

					Button {
						text: "Stop"
						enabled: docroot.timeTracker.isTracking
						onClicked: submitTimeDialog.open()
					}
				}
			}
		}

		GroupBox {
			title: "Recent Time Entries"
			Layout.fillWidth: true
			Layout.fillHeight: true

			ListView {
				anchors.fill: parent
				model: docroot.timeTracker.recentEntries
				clip: true

				delegate: ItemDelegate {
					width: parent.width
					contentItem: ColumnLayout {
						spacing: 5

						RowLayout {
							Layout.fillWidth: true
							Label {
								text: "Id: " + modelData.id
								font.bold: true
							}
							Label {
								text: "Issue #" + modelData.issue_id
								font.bold: true
							}
							Label {
								text: modelData.hours.toFixed(2) + " hours"
								Layout.leftMargin: 10
							}
							Label {
								text: Qt.formatDateTime(modelData.spent_on,
														"yyyy-MM-dd hh:mm")
								Layout.fillWidth: true
								horizontalAlignment: Text.AlignRight
							}
							Label {
								text: modelData.sync_status === 0 ? false : true
								Layout.fillWidth: true
								horizontalAlignment: Text.AlignRight
							}
						}

						Label {
							text: modelData.comments
							visible: modelData.comments !== ""
							wrapMode: Text.WordWrap
							Layout.fillWidth: true
						}
					}
				}

				ScrollBar.vertical: ScrollBar {}
			}
		}
	}

	Dialog {
		id: submitTimeDialog
		title: "Submit Time Entry"
		standardButtons: Dialog.Yes | Dialog.No
		x: (parent.width - width) / 2
		y: (parent.height - height) / 2
		parent: Overlay.overlay
		modal: true

		onAccepted: {
			docroot.timeTracker.stopTracking()

			const hours = docroot.timeTracker.getElapsedHours()
			const issueId = parseInt(issueIdField.text)
			const comments = commentsField.text

			// Add to local history
			docroot.timeTracker.addRecentEntry({
												   "issueId": issueId,
												   "hours": hours,
												   "comments": comments,
												   "date": new Date()
											   })

			// Submit to Redmine
			docroot.client.createTimeEntry(issueId, hours, comments)
			commentsField.text = ""
		}

		onRejected: {
			docroot.timeTracker.stopTracking()
			commentsField.text = ""
		}

		contentItem: ColumnLayout {
			spacing: 10

			Label {
				text: "Would you like to submit this time entry?"
			}

			Label {
				text: "Total time: " + docroot.timeTracker.elapsedTime
				font.bold: true
			}
		}
	}
}
