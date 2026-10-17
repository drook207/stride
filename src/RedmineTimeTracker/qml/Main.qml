pragma ComponentBehavior: Bound

import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtCore
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
    //! Whether reconnectRefetchTimer already caught up since the last server or API key change. Only
    //! once: with an invalid key, a server allowing anonymous access flips between online (anonymous
    //! requests) and offline (the user's), which would otherwise retry endlessly.
    property bool caughtUp: false

    //! Brings the local entries in line with Redmine: asks for the user's time entries on the days
    //! of entries that aren't linked to one or are still to be sent, links the ones Redmine has
    //! (e.g. saved before linking existed, or sent although the app didn't hear back), then sends
    //! the rest (onOwnTimeEntriesReceived). Linking first every time keeps entries from being sent
    //! twice.
    function syncWithRedmine() {
        const range = timeTrackerInstance.unlinkedEntriesRange();
        if (range.from) {
            redmineClient.getOwnTimeEntries(range.from, range.to);
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
            // Links the local entry (its id is the tag, see TimeTracker::submitTimeEntry) so it can be
            // edited on Redmine later. Entries sent again in the background don't need a message.
            if (timeTrackerInstance.linkCreatedTimeEntry(timeEntry, requestTag)) {
                messageDialog.text = qsTr("Time entry created successfully");
                messageDialog.open();
            }
        }
        onTimeEntryCreateFailed: function (requestTag, httpStatus, error) {
            timeTrackerInstance.submissionFailed(requestTag, httpStatus, error);
        }
    }

    Component.onCompleted: {
        redmineClient.detectApiVersion();
        redmineClient.getEnumerations(0);
        redmineClient.getEnumerations(1);
        redmineClient.getEnumerations(2);
        if (!isNaN(timeTrackerInstance.interruptedAt.getTime())) {
            interruptionDialog.open();
        }
        redmineClient.getAllProjects();
        projectAccess.refresh();
        redmineClient.getTimeEntryCustomFields();
        window.syncWithRedmine();
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
            window.caughtUp = false;
            redmineClient.getAllProjects();
            projectAccess.refresh();
            redmineClient.getTimeEntryCustomFields();
            window.syncWithRedmine();
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
                syncTimer.restart();
            }
        }
    }
    // Entries to send when the connection comes back, once the flurry of reconnecting settled.
    Timer {
        id: syncTimer
        interval: 1000
        onTriggered: window.syncWithRedmine()
    }
    // Entries can also fail while the connection stays up (e.g. a server error): try again now and
    // then.
    Timer {
        interval: 2 * 60 * 1000
        repeat: true
        running: redmineClient.connectionState === RedmineClient.ONLINE
        onTriggered: {
            if (timeTrackerInstance.hasPendingEntries()) {
                window.syncWithRedmine();
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

        onSubmitTimeEntry: function (localId, issueId, hours, comments, spentOn, activityId, customFields) {
            redmineClient.createTimeEntry(issueId, hours, comments, spentOn, activityId, customFields, localId);
        }
        onUpdateTimeEntryOnRedmine: function (redmineId, hours, comments, spentOn, activityId, customFields, tag) {
            redmineClient.updateTimeEntry(redmineId, hours, comments, spentOn, activityId, customFields, tag);
        }
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
            timeTrackerInstance.linkExistingTimeEntries(timeEntries);
            timeTrackerInstance.submitPendingEntries();
        }
        function onTimeEntryUpdated(timeEntryId, requestTag) {
            timeTrackerInstance.applyRedmineUpdate(timeEntryId, requestTag);
        }
        function onTimeEntryUpdateFailed(timeEntryId, requestTag) {
            timeTrackerInstance.discardRedmineUpdate(timeEntryId, requestTag);
        }
        function onTimeEntryDeleted(timeEntryId) {
            timeTrackerInstance.removeDeletedTimeEntry(timeEntryId);
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
            reminders: remindersInstance
            trayAvailable: trayIcon.available
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
            reminders: remindersInstance
            trayAvailable: trayIcon.available
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

    TrayIcon {
        id: trayIcon
        timeTracker: timeTrackerInstance
        window: window
    }

    Reminders {
        id: remindersInstance
        timeTracker: timeTrackerInstance
        client: redmineClient
        // As a system notification where there's a tray, else in the window.
        onReminder: (title, message) => {
            if (trayIcon.available) {
                trayIcon.showMessage(title, message);
            } else {
                messageDialog.text = message;
                messageDialog.open();
            }
        }
    }

    // Settings shared with SettingsView (same keys); read when needed, since that view writes them.
    Settings {
        id: appSettings
    }

    // Closing the window can leave the app running in the tray (an option in the settings), e.g. so
    // the tracker keeps going without a window in the way.
    onClosing: close => {
        if (trayIcon.available && String(appSettings.value("tray/closeToTray", false)) === "true") {
            close.accepted = false;
            window.hide();
        }
    }

    // The tracker kept running while the app was closed (or crashed) or the computer was asleep: ask
    // whether that time counts.
    Dialog {
        id: interruptionDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        // Set when opened rather than bound to the window's width, which makes Material's Dialog
        // report a binding loop with the wrapping label (see EditTimeEntryDialog).
        width: 520
        modal: true
        closePolicy: Popup.NoAutoClose
        Overlay.modal: DialogScrim {}
        title: qsTr("Timer still running")
        standardButtons: Dialog.Yes | Dialog.No

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: (timeTrackerInstance.interruptedBySleep
                   ? qsTr("The computer was asleep since %2 while the timer for #%1 was running. Should that time count?")
                   : qsTr("The timer for #%1 was running when the app was last used, on %2. Should the time since then count?"))
                .arg(timeTrackerInstance.currentIssueId)
                .arg(timeTrackerInstance.interruptedAt.toLocaleString(Qt.locale(), Locale.ShortFormat))
        }

        onAboutToShow: {
            width = Math.min(parent.width - 32, 520);
            standardButton(Dialog.Yes).text = qsTr("Count it");
            standardButton(Dialog.No).text = timeTrackerInstance.interruptedBySleep ? qsTr("Leave it out") : qsTr("Stop it then");
        }
        onAccepted: timeTrackerInstance.resolveInterruption(true)
        onRejected: timeTrackerInstance.resolveInterruption(false)
    }
    Connections {
        target: timeTrackerInstance
        function onInterruptedAtChanged() {
            if (!isNaN(timeTrackerInstance.interruptedAt.getTime())) {
                interruptionDialog.open();
            }
        }
    }

    SubmitDialog {
        id: subDialog

        timeTracker: timeTrackerInstance
        client: redmineClient
    }
}
