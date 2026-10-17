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

    //! All projects visible to the user ({id, name, identifier, status, is_public, enabled_modules, ...}).
    property var allProjects: []
    //! Projects to pick from in Add Issue: active ones the user may add issues to.
    readonly property var projects: projectAccess.issueProjects
    //! Whether the user's own time entries were fetched for linking since the last API key change.
    property bool entriesLinked: false
    //! Whether reconnectRefetchTimer already caught up since the last server or API key change. Only
    //! once: with an invalid key, a server allowing anonymous access flips between online (anonymous
    //! requests) and offline (the user's), which would otherwise retry endlessly.
    property bool caughtUp: false

    //! Asks Redmine for the user's time entries on the days of recent entries that aren't linked to
    //! one (e.g. saved before linking existed), so TimeTracker can link them and they can be edited
    //! on Redmine (see TimeTracker::linkExistingTimeEntries).
    function linkUnlinkedEntries() {
        const range = timeTrackerInstance.unlinkedEntriesRange();
        if (range.from) {
            redmineClient.getOwnTimeEntries(range.from, range.to);
        } else {
            window.entriesLinked = true;
        }
    }

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
        projectAccess.refresh();
        redmineClient.getTimeEntryCustomFields();
        window.linkUnlinkedEntries();
        // Recent entries show their issue's project, which needs the issue cached locally. Quietly:
        // the user didn't ask for these, and one they can't fetch (e.g. no access any more) would
        // otherwise report an error on every launch.
        for (const issueId of timeTrackerInstance.issueIdsWithoutProject()) {
            redmineClient.getIssue(issueId, true);
        }
    }

    // Projects, the user's permissions, the time entry custom fields and the user's own time entries
    // depend on the server and API key (the keychain may also deliver the key only after startup's
    // requests): fetch them again after either changed, once typing pauses.
    Timer {
        id: apiKeyRefetchTimer
        interval: 1000
        onTriggered: {
            window.entriesLinked = false;
            window.caughtUp = false;
            redmineClient.getAllProjects();
            projectAccess.refresh();
            redmineClient.getTimeEntryCustomFields();
            window.linkUnlinkedEntries();
        }
    }
    // Fetching fails quietly while offline: when the connection comes back, catch up once on what's
    // still missing (see caughtUp).
    Timer {
        id: reconnectRefetchTimer
        interval: 1000
        onTriggered: {
            if (window.caughtUp) {
                return;
            }
            window.caughtUp = true;
            if (window.allProjects.length === 0) {
                redmineClient.getAllProjects();
            }
            if (!projectAccess.userKnown) {
                projectAccess.refresh();
            }
            if (!window.entriesLinked) {
                window.linkUnlinkedEntries();
            }
        }
    }
    Connections {
        target: redmineClient
        function onApiKeyChanged() {
            apiKeyRefetchTimer.restart();
        }
        function onBaseUrlChanged() {
            apiKeyRefetchTimer.restart();
        }
        function onConnectionStateChanged() {
            if (redmineClient.connectionState === RedmineClient.ONLINE) {
                reconnectRefetchTimer.restart();
            }
        }
    }
    ProjectAccess {
        id: projectAccess
        client: redmineClient
        projects: window.allProjects
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
        function onProjectsReceived(projects) {
            window.allProjects = projects;
            activitiesModelInstance.setProjectActivities(projects);
        }
        function onOwnTimeEntriesReceived(timeEntries) {
            window.entriesLinked = true;
            timeTrackerInstance.linkExistingTimeEntries(timeEntries);
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
