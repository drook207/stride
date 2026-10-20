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

    //! Starts a sync, but at most one per minute: a flapping connection or several triggers at once
    //! would otherwise ask Redmine again and again. A sync that comes during the cooldown is held
    //! back and runs once when it ends. force syncs now regardless and restarts the cooldown.
    function requestSync(force) {
        if (syncCooldown.running && !force) {
            window.syncWanted = true;
            return;
        }
        window.syncWanted = false;
        syncCooldown.restart();
        window.syncWithRedmine();
    }
    //! Whether a sync was requested during the cooldown, see requestSync().
    property bool syncWanted: false
    Timer {
        id: syncCooldown
        interval: 60 * 1000
        onTriggered: {
            if (window.syncWanted) {
                window.requestSync(false);
            }
        }
    }

    //! Requests that depend on the server and API key: at startup (once the key is known) and
    //! again after either changed.
    function fetchServerData(force) {
        redmineClient.getAllProjects();
        projectAccess.refresh();
        redmineClient.getTimeEntryCustomFields();
        window.requestSync(force);
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
        onTimeEntryCreateFailed: function (requestTag, httpStatus, error, retryAfter) {
            timeTrackerInstance.submissionFailed(requestTag, httpStatus, error, retryAfter);
        }
    }

    //! Runs startUp() once the API key is known (the keychain delivers it a moment after launch) and
    //! a server is set (not on a first launch): earlier its requests would go out without them, fail,
    //! and be sent again once they're there.
    function startUpWhenReady() {
        if (window.startedUp || !redmineClient.apiKeyLoaded || redmineClient.baseUrl === "") {
            return;
        }
        window.startedUp = true;
        window.startUp();
    }

    //! Startup requests, see startUpWhenReady().
    function startUp() {
        redmineClient.detectApiVersion();
        redmineClient.getEnumerations(0);
        redmineClient.getEnumerations(1);
        redmineClient.getEnumerations(2);
        window.fetchServerData(false);
        // Recent entries show their issue's project, which needs the issue cached locally. Quietly:
        // the user didn't ask for these, and one they can't fetch (e.g. no access any more) would
        // otherwise report an error on every launch.
        redmineClient.getIssuesById(timeTrackerInstance.issueIdsWithoutProject(), true);
    }

    Component.onCompleted: {
        if (!isNaN(timeTrackerInstance.interruptedAt.getTime())) {
            interruptionDialog.open();
        }
        window.startUpWhenReady();
    }

    //! Whether startUp() ran. The key loaded at startup doesn't count as a change (see
    //! onApiKeyChanged).
    property bool startedUp: false

    // Projects, the user's permissions, the time entry custom fields and the user's own time entries
    // depend on the server and API key (the keychain may also deliver the key only after startup's
    // requests): fetch them again after either changed, once typing pauses.
    Timer {
        id: apiKeyRefetchTimer
        interval: 1000
        onTriggered: {
            window.caughtUp = false;
            window.fetchServerData(true);
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
        function onApiKeyLoadedChanged() {
            window.startUpWhenReady();
        }
        function onApiKeyChanged() {
            // The cached overview belongs to the previous user.
            OverviewCache.clear();
            // Before startUp() this is the initial load, which startUp() covers.
            if (window.startedUp) {
                timeTrackerInstance.resetSubmissionBackoff();
                apiKeyRefetchTimer.restart();
            }
        }
        function onBaseUrlChanged() {
            // The cached overview belongs to the previous server.
            OverviewCache.clear();
            timeTrackerInstance.resetSubmissionBackoff();
            if (window.startedUp) {
                apiKeyRefetchTimer.restart();
            } else {
                // The first server set, e.g. on the first launch
                window.startUpWhenReady();
            }
        }
        function onConnectionStateChanged() {
            if (redmineClient.connectionState === RedmineClient.ONLINE) {
                timeTrackerInstance.connectionRestored();
                reconnectRefetchTimer.restart();
                syncTimer.restart();
            }
        }
    }
    // Entries to send when the connection comes back, once the flurry of reconnecting settled.
    Timer {
        id: syncTimer
        interval: 1000
        onTriggered: window.requestSync(false)
    }
    // Entries can also fail while the connection stays up (e.g. a server error): try again now and
    // then.
    Timer {
        interval: 2 * 60 * 1000
        repeat: true
        running: redmineClient.connectionState === RedmineClient.ONLINE
        onTriggered: {
            if (timeTrackerInstance.hasDueEntries()) {
                window.requestSync(false);
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
        onSetIssueStatus: function (issueId, statusId) {
            redmineClient.updateIssue(issueId, "", "", statusId);
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
        function onIssueNotFound(issueId) {
            timeTrackerInstance.forgetIssue(issueId);
        }
        // Refreshes the cached issue, e.g. after its status was set with a time entry.
        function onIssueUpdated(issueId) {
            redmineClient.getIssue(issueId, true);
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

    // Tells whether an issue is cached locally, see onTimeEntriesReceived.
    IssueModel {
        id: issueCache
    }
    Connections {
        target: timeTrackerInstance
        function onIssueCacheChanged() {
            issueCache.updateIssueList();
        }
        // Overview reports fetched before don't include the change (see OverviewCache).
        function onRecentEntriesChanged() {
            OverviewCache.clear();
        }
    }

    Connections {
        target: redmineClient
        function onTimeEntriesReceived(entries) {
            timeTrackerInstance.addEntriesFromId(entries);
            // Quietly, and only if the issue isn't cached yet.
            if (entries.length > 0 && !issueCache.hasIssue(entries[0].issue.id)) {
                redmineClient.getIssue(entries[0].issue.id, true);
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

    // Shown in the middle of the window while requests run, once they take noticeably long, so
    // quick requests don't make it flash. A plain item rather than a popup: it stays below dialogs
    // (which live on the overlay) and lets clicks through.
    Rectangle {
        id: busyIndicator
        anchors.centerIn: parent
        z: 1
        width: busySpinner.implicitWidth + 32
        height: busySpinner.implicitHeight + 32
        radius: Metrics.panelRadius
        color: Material.dialogColor
        visible: redmineClient.loading && busyDelay.elapsed

        BusyIndicator {
            id: busySpinner
            anchors.centerIn: parent
            running: busyIndicator.visible
        }
    }
    // Restarted by each start of loading (rather than bound to it: a single-shot timer's running
    // turns false when it fires). RedmineClient keeps loading on until the last request finished.
    Timer {
        id: busyDelay
        //! Whether loading has been going on for longer than the interval.
        property bool elapsed: false
        interval: 400
        onTriggered: elapsed = true
    }
    Connections {
        target: redmineClient
        function onLoadingChanged() {
            if (redmineClient.loading) {
                busyDelay.restart();
            } else {
                busyDelay.stop();
                busyDelay.elapsed = false;
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
