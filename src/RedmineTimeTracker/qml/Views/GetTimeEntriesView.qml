import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief "Search Issue" view: fetches and lists every time entry already logged against a given
 * issue, straight from the Redmine REST API (not the local cache) - useful to check what's
 * already been recorded before adding more.
 */
Item {
    id: docroot

    required property TimeTracker timeTracker
    required property RedmineClient client
    //! TimeEntryActivitiesModel for the edit dialog's Activity combo box.
    required property var activitiesModel

    EditTimeEntryDialog {
        id: editDialog

        //! Redmine id of the time entry being edited.
        property int entryId: -1

        activitiesModel: docroot.activitiesModel

        // The list (and any local copies) follow once Redmine confirms the change
        // (TimeTracker.applyRedmineUpdate, wired up in Main.qml).
        onSaved: changes => {
            docroot.timeTracker.stageRedmineUpdate(editDialog.entryId, changes)
            docroot.client.updateTimeEntry(editDialog.entryId, changes.hours, changes.comments, changes.spent_on,
                                           changes.activity_id || 0)
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
            text: qsTr("Search Issue")
        }

        // One card for the search row and its results, like RecentEntriesView's list card.
        Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: 12

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 8
                spacing: 12

                LabeledField {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 400
                    label: qsTr("Issue")

                    IssueSearchInputField {
                        id: issueField
                        Layout.fillWidth: true
                        Layout.preferredHeight: Metrics.controlHeight
                        timeTracker: docroot.timeTracker
                        enabled: !docroot.timeTracker.isTracking
                    }
                }

                PrimaryButton {
                    Layout.alignment: Qt.AlignBottom
                    text: qsTr("Get")
                    enabled: issueField.acceptableInput
                    onClicked: docroot.client.getTimeEntries(issueField.value)
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Material.dividerColor
            }

            ListView {
                id: entriesList
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: count > 0
                model: docroot.timeTracker.entriesFromId
                clip: true

                delegate: TimeEntryDelegate {
                    required property var modelData
                    required property int index

                    entryId: modelData.id
                    issueId: modelData.issue.id
                    projectName: modelData.project ? modelData.project.name : ""
                    hours: modelData.hours
                    comment: modelData.comments || ""
                    spentOn: modelData.spent_on
                    showDivider: index < ListView.view.count - 1
                    editable: true

                    onClicked: {
                        editDialog.entryId = modelData.id
                        editDialog.openFor({
                                               "issueId": modelData.issue.id,
                                               "hours": modelData.hours,
                                               "comments": modelData.comments,
                                               "spentOn": modelData.spent_on,
                                               "activityId": modelData.activity ? modelData.activity.id : 0,
                                               "localOnly": false
                                           })
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }

            // Empty state
            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: entriesList.count === 0
                text: qsTr("Enter an issue and press Get to list its time entries.")
                color: Material.secondaryTextColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
