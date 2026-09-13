import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QRedmine.TimeTracker
import QRedmine.Client

ApplicationWindow {
    id: window
    visible: true
    width: 1280
    height: 960
    title: Application.name

    Material.theme: Material.Dark
    Material.accent: "#9F8FEF"

    RedmineClient {
        id: redmineClient

        onErrorOccurred: function (error) {
            messageDialog.text = "Error: " + error;
            messageDialog.open();
        }

        onTimeEntryCreated: function (timeEntry) {
            messageDialog.text = "Time entry created successfully";
            messageDialog.open();
        }
    }

    Component.onCompleted: {
        redmineClient.detectApiVersion();
        redmineClient.getEnumerations(0);
        redmineClient.getEnumerations(1);
        redmineClient.getEnumerations(2);
    }
    TimeTracker {
        id: timeTrackerInstance
    }
    TimeEntryActivitiesModel {
        id: activitiesModel
    }
    Connections {
        target: redmineClient
        function onTimeEntryActivitiesReceived(value) {
            activitiesModel.addTimeEntryActivities(value);
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

    Header {
        id: header
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            topMargin: 5
        }

        connectionState: redmineClient.connectionState

        onMenuToggled: sidebar.expanded = !sidebar.expanded
    }

    ColumnLayout {
        anchors {
            top: header.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        anchors.margins: 10
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            Sidebar {
                id: sidebar
                Layout.fillHeight: true

                view: stackView
                timeTracker: timeTrackerInstance
                componentsToLoad: [recentEntriesView, addTimeEntryView, getTimeEntriesView, addIssueView, settingsView]
                buttonTitles: [qsTr("Recent Entries"), qsTr("Add Entry"), qsTr("Search Issue"), qsTr("Add Issue")]
            }

            StackView {
                id: stackView
                Layout.fillWidth: true
                Layout.fillHeight: true
                initialItem: recentEntriesView
            }
        }

        TrackingBar {
            id: trackingBar
            Layout.fillWidth: true

            timeTracker: timeTrackerInstance
            client: redmineClient
            submitDialog: subDialog
            activitiesModel: activitiesModel
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

    Component {
        id: recentEntriesView
        RecentEntriesView {
            timeTracker: timeTrackerInstance
        }
    }

    Component {
        id: addTimeEntryView
        AddTimeEntryView {
            timeTracker: timeTrackerInstance
            client: redmineClient
        }
    }

    Component {
        id: getTimeEntriesView

        GetTimeEntriesView {
            timeTracker: timeTrackerInstance
            client: redmineClient
        }
    }

    Component {
        id: addIssueView

        AddIssueView {
            client: redmineClient
        }
    }

    Component {
        id: settingsView

        SettingsView {
            timeTracker: timeTrackerInstance
            client: redmineClient
            submitDialog: subDialog
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
