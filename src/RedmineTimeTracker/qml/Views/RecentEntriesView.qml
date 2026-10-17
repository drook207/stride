import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief Default/home view: lists locally tracked time entries (TimeTracker's local SQLite-backed
 * recentEntries model), newest first.
 */
Item {
	id: docroot

	required property TimeTracker timeTracker
	required property RedmineClient client
	//! TimeEntryActivitiesModel for the edit dialog's Activity combo box.
	required property var activitiesModel
	//! The time entry custom field "Special" (Main.qml), or null if the server has none.
	property var specialField: null

	//! Project name to show entries of, or "" for all.
	property string projectFilter: ""
	//! Whether entries are grouped by project (projects alphabetically, newest first within each)
	//! rather than listed newest first.
	property bool groupByProject: false

	//! Project name of an entry, "" if its issue isn't cached with one (yet).
	function projectOf(entry) {
		return entry.project_name || ""
	}

	//! Projects that recent entries belong to, alphabetically.
	readonly property var projectNames: {
		const names = []
		for (const entry of docroot.timeTracker.recentEntries) {
			const name = docroot.projectOf(entry)
			if (name.length > 0 && names.indexOf(name) < 0) {
				names.push(name)
			}
		}
		return names.sort((a, b) => a.localeCompare(b))
	}

	//! The recent entries as listed: filtered by projectFilter and, with groupByProject, sorted
	//! by project (entries without one last) while keeping the newest-first order within each.
	readonly property var shownEntries: {
		const entries = docroot.timeTracker.recentEntries.filter(
						  entry => docroot.projectFilter === "" || docroot.projectOf(entry) === docroot.projectFilter)
		if (!docroot.groupByProject) {
			return entries
		}
		return entries.map((entry, index) => ({
													  "entry": entry,
													  "index": index
												  })).sort((a, b) => {
														  const pa = docroot.projectOf(a.entry)
														  const pb = docroot.projectOf(b.entry)
														  if (pa !== pb) {
															  if (pa === "") {
																  return 1
															  }
															  if (pb === "") {
																  return -1
															  }
															  return pa.localeCompare(pb)
														  }
														  return a.index - b.index
													  }).map(item => item.entry)
	}

	// Drop a filter whose project no longer has entries.
	onProjectNamesChanged: {
		if (docroot.projectFilter !== "" && docroot.projectNames.indexOf(docroot.projectFilter) < 0) {
			docroot.projectFilter = ""
		}
	}

	EditTimeEntryDialog {
		id: editDialog

		//! The entry being edited, as listed in TimeTracker.recentEntries.
		property var entry: ({})

		activitiesModel: docroot.activitiesModel
		specialField: docroot.specialField

		onSaved: changes => {
			const redmineId = editDialog.entry.redmine_id
			if (redmineId > 0) {
				// Linked to a Redmine time entry: change it there; the local copy follows once Redmine
				// confirms (TimeTracker.applyRedmineUpdate, wired up in Main.qml).
				docroot.timeTracker.stageRedmineUpdate(redmineId, changes)
				docroot.client.updateTimeEntry(redmineId, changes.hours, changes.comments, changes.spent_on,
											   changes.activity_id || 0, changes.custom_fields || [])
			} else {
				docroot.timeTracker.updateRecentEntry(editDialog.entry.id, changes)
			}
		}
	}

	ColumnLayout {
		anchors {
			fill: parent
			margins: Metrics.pageMargin
		}
		spacing: Metrics.sectionSpacing

		PageTitle {
			Layout.fillWidth: true
			text: qsTr("Recent Entries")
		}

		RowLayout {
			Layout.fillWidth: true
			Layout.topMargin: -4
			spacing: 12
			visible: docroot.projectNames.length > 0

			ComboBox {
				id: projectFilterCombo
				// Shrinks on narrow (mobile) windows so the switch still fits next to it.
				Layout.fillWidth: true
				Layout.maximumWidth: 220
				Layout.preferredHeight: Metrics.controlHeight
				topInset: 0
				bottomInset: 0
				textRole: "name"
				valueRole: "value"
				model: [{
						"name": qsTr("All projects"),
						"value": ""
					}].concat(docroot.projectNames.map(name => ({
																	"name": name,
																	"value": name
																})))
				// Re-sync after the model is rebuilt (new entries, retranslation), which resets it.
				onModelChanged: currentIndex = Math.max(0, indexOfValue(docroot.projectFilter))
				onActivated: docroot.projectFilter = currentValue
			}

			Switch {
				text: qsTr("Group by project")
				checked: docroot.groupByProject
				onToggled: docroot.groupByProject = checked
			}
		}

		Card {
			Layout.fillWidth: true
			Layout.fillHeight: true
			padding: 8

			ListView {
				id: recentEntries
				Layout.fillWidth: true
				Layout.fillHeight: true
				visible: count > 0
				model: docroot.shownEntries
				clip: true

				delegate: ColumnLayout {
					id: entryRow

					required property var modelData
					required property int index
					//! Whether this entry is the first of its project's group, so the header goes here.
					readonly property bool startsGroup: docroot.groupByProject
														&& (index === 0 || docroot.projectOf(docroot.shownEntries[index - 1])
															!== docroot.projectOf(modelData))

					width: ListView.view.width
					spacing: 0

					Label {
						visible: entryRow.startsGroup
						Layout.topMargin: entryRow.index > 0 ? 12 : 4
						Layout.bottomMargin: 2
						leftPadding: 12
						text: docroot.projectOf(entryRow.modelData) || qsTr("No project")
						color: Material.accentColor
						font.pixelSize: 12
						font.weight: Font.DemiBold
					}

					TimeEntryDelegate {
						readonly property var modelData: entryRow.modelData
						readonly property int index: entryRow.index

						Layout.fillWidth: true
						projectName: docroot.groupByProject ? "" : docroot.projectOf(modelData)
						issueId: modelData.issue_id
						hours: modelData.hours
						comment: modelData.comments || ""
						spentOn: modelData.spent_on
						showDivider: index < docroot.shownEntries.length - 1
						editable: true

						onClicked: {
							editDialog.entry = modelData
							editDialog.openFor({
												   "issueId": modelData.issue_id,
											   "projectId": modelData.project_id || -1,
												   "hours": modelData.hours,
												   "comments": modelData.comments,
												   "spentOn": modelData.spent_on,
												   "activityId": 0,
												   "localOnly": !(modelData.redmine_id > 0)
											   })
						}
					}
				}

				ScrollBar.vertical: ScrollBar {}
			}

			// Empty state
			ColumnLayout {
				Layout.fillWidth: true
				Layout.fillHeight: true
				Layout.margins: 12
				visible: recentEntries.count === 0
				spacing: 6

				Item {
					Layout.fillHeight: true
				}
				Label {
					Layout.fillWidth: true
					text: qsTr("No time entries yet")
					color: Material.foreground
					font.pixelSize: 15
					font.weight: Font.DemiBold
					horizontalAlignment: Text.AlignHCenter
				}
				Label {
					Layout.fillWidth: true
					text: qsTr("Track time with the bar below or add an entry manually.")
					color: Material.secondaryTextColor
					horizontalAlignment: Text.AlignHCenter
					wrapMode: Text.WordWrap
				}
				Item {
					Layout.fillHeight: true
				}
			}
		}
	}
}
