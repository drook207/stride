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
    //! The user the API key belongs to, with "memberships" (see RedmineClient::getCurrentUser), or
    //! null while unknown.
    property var currentUser: null
    //! Permissions of the user's roles by role id, as far as fetched (RedmineClient::getRole).
    property var rolePermissions: ({})
    //! Redmine's built-in "Non member" role, which applies in public projects the user isn't a member
    //! of. Created by Redmine's installation before any other role, so its id is 1.
    readonly property int nonMemberRoleId: 1

    //! Projects to pick from in Add Issue: active ones the user may add issues to.
    readonly property var projects: window.allProjects.filter(project => project.status === 1 && window.canAddIssues(project))

    //! Asks Redmine for the user's time entries on the days of recent entries that aren't linked to
    //! one (e.g. saved before linking existed), so TimeTracker can link them and they can be edited
    //! on Redmine (see TimeTracker::linkExistingTimeEntries).
    function linkUnlinkedEntries() {
        const range = timeTrackerInstance.unlinkedEntriesRange();
        if (range.from) {
            redmineClient.getOwnTimeEntries(range.from, range.to);
        }
    }

    //! Whether the user may add issues to a project. While the user or a role's permissions aren't
    //! known (yet, or fetching them failed), projects are assumed to allow it rather than hidden.
    function canAddIssues(project) {
        if (project.enabled_modules && !project.enabled_modules.some(module => module.name === "issue_tracking")) {
            return false;
        }
        if (!window.currentUser || window.currentUser.admin) {
            return true;
        }
        const roleAllows = roleId => {
            const permissions = window.rolePermissions[roleId];
            return permissions === undefined || permissions.indexOf("add_issues") >= 0;
        };
        const membership = (window.currentUser.memberships || []).find(m => m.project && m.project.id === project.id);
        if (membership) {
            return membership.roles.some(role => roleAllows(role.id));
        }
        return project.is_public === true && roleAllows(window.nonMemberRoleId);
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
        redmineClient.getCurrentUser();
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
    // depend on the API key (the keychain may also deliver it only after startup's requests), and
    // fetching projects fails while offline: fetch them again after the key changed, once typing
    // pauses, and projects when the connection comes back without them.
    Timer {
        id: apiKeyRefetchTimer
        interval: 1000
        onTriggered: {
            redmineClient.getAllProjects();
            redmineClient.getCurrentUser();
            redmineClient.getTimeEntryCustomFields();
            window.linkUnlinkedEntries();
        }
    }
    Timer {
        id: projectsRefetchTimer
        interval: 1000
        onTriggered: redmineClient.getAllProjects()
    }
    Connections {
        target: redmineClient
        function onApiKeyChanged() {
            apiKeyRefetchTimer.restart();
        }
        function onConnectionStateChanged() {
            if (redmineClient.connectionState === RedmineClient.ONLINE && window.allProjects.length === 0) {
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
        function onProjectsReceived(projects) {
            window.allProjects = projects;
            activitiesModelInstance.setProjectActivities(projects);
        }
        function onOwnTimeEntriesReceived(timeEntries) {
            timeTrackerInstance.linkExistingTimeEntries(timeEntries);
        }
        function onCurrentUserReceived(user) {
            window.currentUser = user;
            window.rolePermissions = {};
            const roleIds = [window.nonMemberRoleId];
            for (const membership of user.memberships || []) {
                for (const role of membership.roles) {
                    if (roleIds.indexOf(role.id) < 0) {
                        roleIds.push(role.id);
                    }
                }
            }
            if (!user.admin) {
                roleIds.forEach(roleId => redmineClient.getRole(roleId));
            }
        }
        function onRoleReceived(role) {
            const permissions = Object.assign({}, window.rolePermissions);
            permissions[role.id] = role.permissions || [];
            window.rolePermissions = permissions;
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
