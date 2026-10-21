pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Stride
import QRedmine.Client

/*!
 * \brief Week overview: hours booked per day against the daily target, and per project.
 *
 * Counts the user's time entries on Redmine plus the ones not sent yet (see TimeOverview), fetched
 * again whenever the week or the local entries change.
 */
Item {
	id: docroot

	required property TimeTracker timeTracker
	required property RedmineClient client

	//! Asks Redmine for the shown week's entries; the answer arrives via timeEntryReportReceived.
	//! Reuses a report fetched a moment ago (see OverviewCache) unless force is set.
	function fetch(force) {
		const week = overview.range()
		const cached = force ? null : OverviewCache.lookup(week.from, week.to)
		if (cached) {
			overview.setRemoteEntries(cached, week.from, week.to)
			return
		}
		docroot.client.getTimeEntryReport(week.from, week.to, RedmineClient.OverviewReport)
	}

	//! "7,5 h" style, with the decimal comma used by the hour fields.
	function formatHours(hours) {
		return qsTr("%1 h").arg(HoursFormat.format(hours))
	}

	TimeOverview {
		id: overview
		onRangeChanged: docroot.fetch()
	}

	ExportDialog {
		id: exportDialog
		client: docroot.client
	}

	Connections {
		target: docroot.client
		function onTimeEntryReportReceived(timeEntries, from, to, purpose) {
			if (purpose === RedmineClient.OverviewReport) {
				OverviewCache.store(from, to, timeEntries)
				overview.setRemoteEntries(timeEntries, from, to)
			}
		}
	}

	// A new, changed, sent or deleted entry changes the week: count again right away, and ask
	// Redmine again once a burst of changes (e.g. several entries sent on reconnecting) settled.
	Connections {
		target: docroot.timeTracker
		function onRecentEntriesChanged() {
			overview.refresh()
			refetchTimer.restart()
		}
	}
	Timer {
		id: refetchTimer
		interval: 3000
		onTriggered: docroot.fetch(true)
	}

	Component.onCompleted: docroot.fetch()

	ColumnLayout {
		anchors {
			fill: parent
			margins: Metrics.pageMargin
		}
		spacing: Metrics.sectionSpacing

		PageTitle {
			Layout.fillWidth: true
			text: qsTr("Overview")
		}

		ScrollView {
			Layout.fillWidth: true
			Layout.fillHeight: true
			contentWidth: availableWidth

			ColumnLayout {
				width: parent.width
				spacing: Metrics.sectionSpacing

				Card {
					Layout.fillWidth: true

					RowLayout {
						Layout.fillWidth: true
						spacing: 8

						ToolButton {
							icon.source: "qrc:/qt/qml/Stride/qml/Assets/Icons/solid/chevron-left.svg"
							icon.width: 14
							icon.height: 14
							onClicked: overview.previousWeek()
							ToolTip.visible: hovered
							ToolTip.text: qsTr("Previous week")
						}
						Label {
							text: qsTr("Week %1 · %2 – %3").arg(overview.weekNumber)
								.arg(overview.weekStart.toLocaleDateString(Qt.locale(), "dd.MM."))
								.arg(overview.weekEnd.toLocaleDateString(Qt.locale(), "dd.MM.yyyy"))
							color: Material.foreground
							font.pixelSize: 15
							font.weight: Font.DemiBold
						}
						ToolButton {
							icon.source: "qrc:/qt/qml/Stride/qml/Assets/Icons/solid/chevron-right.svg"
							icon.width: 14
							icon.height: 14
							onClicked: overview.nextWeek()
							ToolTip.visible: hovered
							ToolTip.text: qsTr("Next week")
						}
						Button {
							text: qsTr("This week")
							flat: true
							onClicked: overview.showWeekOf(new Date())
						}
						Item {
							Layout.fillWidth: true
						}
						Button {
							text: qsTr("Export…")
							flat: true
							onClicked: exportDialog.openFor(overview.weekStart, overview.weekEnd)
						}
						Label {
							text: qsTr("%1 of %2").arg(docroot.formatHours(overview.totalHours)).arg(docroot.formatHours(overview.targetHours))
							color: Material.foreground
							font.pixelSize: 15
						}
					}

					// One bar per day, against the day's target (dashed line).
					RowLayout {
						id: bars
						Layout.fillWidth: true
						Layout.preferredHeight: 180
						spacing: 8

						//! Hours the full bar height stands for.
						readonly property real fullHours: Math.max(overview.dailyTarget * 1.25,
															   ...overview.days.map(day => day.hours), 1)

						Repeater {
							model: overview.days

							ColumnLayout {
								id: dayColumn
								required property var modelData
								readonly property real fill: modelData.hours / bars.fullHours

								Layout.fillWidth: true
								Layout.fillHeight: true
								spacing: 4

								Label {
									Layout.alignment: Qt.AlignHCenter
									text: dayColumn.modelData.hours > 0 ? HoursFormat.format(dayColumn.modelData.hours) : ""
									color: Material.secondaryTextColor
									font.pixelSize: 12
								}

								Item {
									Layout.fillWidth: true
									Layout.fillHeight: true

									Rectangle {
										anchors {
											bottom: parent.bottom
											horizontalCenter: parent.horizontalCenter
										}
										width: Math.min(parent.width, 44)
										height: parent.height * dayColumn.fill
										radius: 4
										color: Material.accentColor
										opacity: dayColumn.modelData.isFuture ? 0.4 : 1
									}

									// The day's target.
									Rectangle {
										visible: dayColumn.modelData.target > 0
										anchors.horizontalCenter: parent.horizontalCenter
										width: Math.min(parent.width, 56)
										height: 1
										y: parent.height * (1 - dayColumn.modelData.target / bars.fullHours)
										color: Material.hintTextColor
									}
								}

								Label {
									Layout.alignment: Qt.AlignHCenter
									text: new Date(dayColumn.modelData.date + "T00:00:00").toLocaleDateString(Qt.locale(), "ddd")
									color: dayColumn.modelData.isToday ? Material.accentColor : Material.secondaryTextColor
									font.weight: dayColumn.modelData.isToday ? Font.DemiBold : Font.Normal
								}
							}
						}
					}

					Label {
						Layout.fillWidth: true
						visible: overview.missingHours > 0.005
						text: qsTr("%1 still to book on the workdays so far.").arg(docroot.formatHours(overview.missingHours))
						color: Material.color(Material.Orange, Material.Shade200)
						wrapMode: Text.WordWrap
					}
					Label {
						Layout.fillWidth: true
						visible: !overview.includesRedmine
						text: qsTr("Only the entries saved in the app are counted, since Redmine couldn't be asked.")
						color: Material.hintTextColor
						font.pixelSize: 12
						wrapMode: Text.WordWrap
					}

					RowLayout {
						spacing: 8
						Label {
							text: qsTr("Target per workday")
							color: Material.secondaryTextColor
						}
						SpinBox {
							from: 0
							to: 24
							value: Math.round(overview.dailyTarget)
							editable: true
							onValueModified: overview.dailyTarget = value
						}
					}
				}

				Card {
					Layout.fillWidth: true
					title: qsTr("Projects")
					visible: overview.projects.length > 0

					Repeater {
						model: overview.projects

						RowLayout {
							id: projectRow
							required property var modelData

							Layout.fillWidth: true
							spacing: 12

							Label {
								Layout.preferredWidth: 180
								text: projectRow.modelData.name || qsTr("No project")
								color: Material.foreground
								elide: Text.ElideRight
							}
							Item {
								Layout.fillWidth: true
								implicitHeight: 10

								Rectangle {
									width: parent.width * projectRow.modelData.hours / Math.max(overview.totalHours, 0.01)
									height: parent.height
									radius: 4
									color: Material.accentColor
								}
							}
							Label {
								Layout.preferredWidth: 64
								horizontalAlignment: Text.AlignRight
								text: docroot.formatHours(projectRow.modelData.hours)
								color: Material.secondaryTextColor
							}
						}
					}
				}
			}
		}
	}
}
