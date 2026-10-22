pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Stride
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
    //! The time entry custom field "Special" (Main.qml), or null if the server has none.
    property var specialField: null

    //! Value of the Special field on a time entry from Redmine: "" if empty, undefined if the entry
    //! doesn't have the field (so editing leaves it unchanged rather than clearing it).
    function specialOf(entry) {
        if (!docroot.specialField) {
            return undefined
        }
        const field = (entry.custom_fields || []).find(f => f.id === docroot.specialField.id)
        return field ? (field.value || "") : undefined
    }

    //! Relations of the issue whose time entries are listed, as RedmineClient::issueRelationsReceived
    //! hands them on; empty while none are known.
    property var relations: []
    //! The issue the listed time entries and relations belong to, -1 until something was fetched.
    property int shownIssueId: -1

    //! Loads an issue's time entries and relations, as "Get" and a click on a related issue do.
    function show(issueId) {
        if (issueId <= 0) {
            return
        }
        docroot.shownIssueId = issueId
        docroot.relations = []
        issueField.value = issueId
        docroot.client.getTimeEntries(issueId)
        docroot.client.getIssueRelations(issueId)
    }

    //! How a relation reads from the shown issue's side, e.g. "blocks" or "blocked by".
    function relationLabel(type) {
        switch (type) {
        case "relates":
            return qsTr("relates to")
        case "duplicates":
            return qsTr("duplicates")
        case "duplicated":
            return qsTr("duplicated by")
        case "blocks":
            return qsTr("blocks")
        case "blocked":
            return qsTr("blocked by")
        case "precedes":
            return qsTr("precedes")
        case "follows":
            return qsTr("follows")
        case "copied_to":
            return qsTr("copied to")
        case "copied_from":
            return qsTr("copied from")
        }
        return type
    }

    // Subjects of related issues: a relation holds only their ids; the subjects come from the issue
    // cache, which Main.qml fills from the getIssuesById() request below.
    IssueModel {
        id: relatedIssues
    }
    Connections {
        target: docroot.timeTracker
        function onIssueCacheChanged() {
            relatedIssues.updateIssueList()
        }
    }
    Connections {
        target: docroot.client
        function onIssueRelationsReceived(issueId, relations) {
            if (issueId !== docroot.shownIssueId) {
                return // another issue was asked for meanwhile
            }
            docroot.relations = relations
            if (relations.length > 0) {
                docroot.client.getIssuesById(relations.map(relation => relation.other_issue_id), true)
            }
        }
    }

    EditTimeEntryDialog {
        id: editDialog

        //! Redmine id of the time entry being edited.
        property int entryId: -1

        activitiesModel: docroot.activitiesModel
        specialField: docroot.specialField

        // The list (and any local copies) follow once Redmine confirms the change
        // (TimeTracker.applyRedmineUpdate, wired up in Main.qml).
        onSaved: changes => {
            const tag = docroot.timeTracker.stageRedmineUpdate(editDialog.entryId, changes)
            docroot.client.updateTimeEntry(editDialog.entryId, changes.hours, changes.comments, changes.spent_on,
                                           changes.activity_id || 0, changes.custom_fields || [], tag)
        }

        // The list (and any local copies) follow once Redmine confirms
        // (TimeTracker.removeDeletedTimeEntry, wired up in Main.qml).
        onDeleteRequested: docroot.client.deleteTimeEntry(editDialog.entryId)
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

        // Relations of the shown issue (blocks, relates, ...): click one to show its time entries, or
        // start tracking it right away.
        Card {
            Layout.fillWidth: true
            visible: docroot.relations.length > 0
            title: qsTr("Related issues")

            ListView {
                id: relationsList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 180)
                clip: true
                interactive: contentHeight > height
                model: docroot.relations

                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    id: relationRow
                    required property var modelData

                    width: ListView.view.width
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Show this issue's time entries")
                    onClicked: docroot.show(relationRow.modelData.other_issue_id)

                    contentItem: RowLayout {
                        spacing: 12

                        Label {
                            Layout.preferredWidth: 110
                            text: docroot.relationLabel(relationRow.modelData.other_relation_type)
                            color: Material.accentColor
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            // Reading revision makes the binding re-evaluate when the issue cache
                            // changes: idAndSubjectOf() is a function QML can't see the result of.
                            text: relatedIssues.revision >= 0 ? relatedIssues.idAndSubjectOf(relationRow.modelData.other_issue_id) : ""
                            color: Material.foreground
                            font.pixelSize: 14
                            elide: Text.ElideRight
                        }
                        ToolButton {
                            icon.source: "qrc:/qt/qml/Stride/qml/Assets/Icons/solid/play.svg"
                            icon.width: 14
                            icon.height: 14
                            enabled: !docroot.timeTracker.isTracking
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Start tracking this issue")
                            onClicked: {
                                docroot.timeTracker.continueTracking(relationRow.modelData.other_issue_id)
                                // Quietly cached, so the tracking bar knows the issue's subject and project.
                                docroot.client.getIssue(relationRow.modelData.other_issue_id, true)
                            }
                        }
                    }
                }
            }
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
                    onClicked: docroot.show(issueField.value)
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
                    special: docroot.specialOf(modelData) || ""
                    hours: modelData.hours
                    comment: modelData.comments || ""
                    spentOn: modelData.spent_on
                    showDivider: index < ListView.view.count - 1
                    editable: true
                    continuable: !docroot.timeTracker.isTracking
                    showFavorite: true
                    favorite: docroot.timeTracker.favoriteIssues.some(f => f.issue_id === modelData.issue.id)

                    onContinueRequested: docroot.timeTracker.continueTracking(modelData.issue.id, modelData.comments || "",
                                                                          modelData.activity ? modelData.activity.id : 0)
                    onFavoriteToggled: docroot.timeTracker.toggleFavoriteIssue(modelData.issue.id)

                    onClicked: {
                        editDialog.entryId = modelData.id
                        editDialog.openFor({
                                               "issueId": modelData.issue.id,
                                               "projectId": modelData.project ? modelData.project.id : -1,
                                               "hours": modelData.hours,
                                               "comments": modelData.comments,
                                               "spentOn": modelData.spent_on,
                                               "activityId": modelData.activity ? modelData.activity.id : 0,
                                               "special": docroot.specialOf(modelData)
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
                text: docroot.shownIssueId > 0 ? qsTr("No time entries for this issue yet.")
                                               : qsTr("Enter an issue and press Get to list its time entries.")
                color: Material.secondaryTextColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
