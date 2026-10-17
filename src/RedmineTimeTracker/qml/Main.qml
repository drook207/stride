import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief Application entry point and root window.
 *
 * Owns the backend instances shared by the whole UI (RedmineClient, TimeTracker, the time entry
 * activities model) and their wiring, then delegates the actual visual layout to either
 * DesktopShell or MobileShell via a Loader, picked once at startup based on isMobile.
 */
ApplicationWindow {
    id: window
    visible: true
    width: 1280
    height: 960
    visibility: window.isMobile ? Window.FullScreen : Window.AutomaticVisibility
    title: Application.name

    Material.theme: Material.Dark
    Material.accent: "#9F8FEF"

    //! Chooses the UI shell (Desktop/Mobile) once at startup. Deliberately platform-based rather
    //! than screen-size/aspect-ratio-based, since a resized desktop window should keep the desktop
    //! layout rather than flip to the mobile one.
    readonly property bool isMobile: Qt.platform.os === "android" || Qt.platform.os === "ios"

    //! Redmine's active projects ({id, name, identifier, ...}), for picking one in Add Issue.
    property var projects: []
    //! The time entry custom field named "Special" (see RedmineClient::getTimeEntryCustomFields),
    //! or null if the server has none; drives the Special combo boxes.
    property var specialField: null

    RedmineClient {
        id: redmineClient

        onErrorOccurred: function (error) {
            messageDialog.text = qsTr("Error: %1").arg(error);
            messageDialog.open();
        }

        onTimeEntryCreated: function (timeEntry, requestTag) {
            // Lets the local entry be edited on Redmine later: the dialogs pass its local id as the
            // tag, see TimeTracker::linkCreatedTimeEntry.
            timeTrackerInstance.linkCreatedTimeEntry(timeEntry, requestTag);
            messageDialog.text = qsTr("Time entry created successfully");
            messageDialog.open();
        }
    }

    Component.onCompleted: {
        redmineClient.detectApiVersion();
        redmineClient.getEnumerations(0);
        redmineClient.getEnumerations(1);
        redmineClient.getEnumerations(2);
        redmineClient.getAllProjects();
        redmineClient.getTimeEntryCustomFields();
        // Recent entries show their issue's project, which needs the issue cached locally. Quietly:
        // the user didn't ask for these, and one they can't fetch (e.g. no access any more) would
        // otherwise report an error on every launch.
        for (const issueId of timeTrackerInstance.issueIdsWithoutProject()) {
            redmineClient.getIssue(issueId, true);
        }
    }

    // Projects depend on the API key, and fetching them fails while offline: fetch them again
    // after the key changed (once typing pauses) and when the connection comes back without them.
    Timer {
        id: projectsRefetchTimer
        interval: 1000
        onTriggered: redmineClient.getAllProjects()
    }
    Connections {
        target: redmineClient
        function onApiKeyChanged() {
            projectsRefetchTimer.restart();
        }
        function onConnectionStateChanged() {
            if (redmineClient.connectionState === RedmineClient.ONLINE && window.projects.length === 0) {
                projectsRefetchTimer.restart();
            }
        }
    }
    // The "Instance" suffix is deliberate: the shells below have properties named timeTracker and
    // activitiesModel, and inside `DesktopShell { activitiesModel: activitiesModel }` the right-hand
    // side resolves to the shell's own property rather than an id here, binding it to itself.
    TimeTracker {
        id: timeTrackerInstance
    }
    TimeEntryActivitiesModel {
        id: activitiesModelInstance
    }
    Connections {
        target: redmineClient
        function onTimeEntryCustomFieldsReceived(fields) {
            window.specialField = fields.find(field => field.name.toLowerCase() === "special") || null;
        }
        // The API key is read from the keychain asynchronously, possibly after startup's requests;
        // the custom fields depend on who is asking (see getTimeEntryCustomFields), so ask again.
        function onApiKeyChanged() {
            redmineClient.getTimeEntryCustomFields();
        }
        function onProjectsReceived(projects) {
            // Closed and archived projects don't take new issues.
            window.projects = projects.filter(project => project.status === 1);
            activitiesModelInstance.setProjectActivities(projects);
        }
        function onTimeEntryUpdated(timeEntryId, requestTag) {
            timeTrackerInstance.applyRedmineUpdate(timeEntryId, requestTag);
        }
        function onTimeEntryUpdateFailed(timeEntryId, requestTag) {
            timeTrackerInstance.discardRedmineUpdate(timeEntryId, requestTag);
        }
        function onTimeEntryActivitiesReceived(value) {
            activitiesModelInstance.addTimeEntryActivities(value);
        }
    }

    Connections {
        target: redmineClient
        function onIssueReceived(issue) {
            timeTrackerInstance.addIssueToDatabase(issue);
        }
    }

    Connections {
        target: redmineClient
        function onTimeEntriesReceived(entries) {
            timeTrackerInstance.addEntriesFromId(entries);
            if (entries.length > 0) {
                redmineClient.getIssue(entries[0].issue.id);
            }
        }
    }

    Loader {
        anchors.fill: parent
        sourceComponent: window.isMobile ? mobileShellComponent : desktopShellComponent
    }

    Component {
        id: desktopShellComponent
        DesktopShell {
            client: redmineClient
            timeTracker: timeTrackerInstance
            activitiesModel: activitiesModelInstance
            projects: window.projects
            specialField: window.specialField
            submitDialog: subDialog
        }
    }

    Component {
        id: mobileShellComponent
        MobileShell {
            client: redmineClient
            timeTracker: timeTrackerInstance
            activitiesModel: activitiesModelInstance
            projects: window.projects
            specialField: window.specialField
            submitDialog: subDialog
        }
    }

    Popup {
        id: busyIndicator
        BusyIndicator {
            running: redmineClient.loading

            onRunningChanged: {
                if (running) {
                    busyIndicator.open();
                } else {
                    busyIndicator.close();
                }
            }
        }
    }

    MessageDialog {
        id: messageDialog
    }

    SubmitDialog {
        id: subDialog

        timeTracker: timeTrackerInstance
        client: redmineClient
    }
}
