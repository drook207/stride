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

    RedmineClient {
        id: redmineClient

        onErrorOccurred: function (error) {
            messageDialog.text = qsTr("Error: %1").arg(error);
            messageDialog.open();
        }

        onTimeEntryCreated: function (timeEntry) {
            // Lets the local entry be edited on Redmine later, see TimeTracker::linkCreatedTimeEntry.
            timeTrackerInstance.linkCreatedTimeEntry(timeEntry);
            messageDialog.text = qsTr("Time entry created successfully");
            messageDialog.open();
        }
    }

    Component.onCompleted: {
        redmineClient.detectApiVersion();
        redmineClient.getEnumerations(0);
        redmineClient.getEnumerations(1);
        redmineClient.getEnumerations(2);
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
        function onTimeEntryUpdated(timeEntryId) {
            timeTrackerInstance.applyRedmineUpdate(timeEntryId);
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
            submitDialog: subDialog
        }
    }

    Component {
        id: mobileShellComponent
        MobileShell {
            client: redmineClient
            timeTracker: timeTrackerInstance
            activitiesModel: activitiesModelInstance
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
