import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Effects
import QRedmine.TimeTracker
import QRedmine.Client

ApplicationWindow {
    id: window
    visible: true
    width: 600
    height: 1024
    title: "Redmine Time Tracker"

    Material.theme: Material.Dark
    Material.accent: Material.LightGreen

    RedmineClient {
        id: redmineClient
        baseUrl: redmineClient.baseUrl
                 === "" ? "https://your-redmine-instance.com" : redmineClient.baseUrl
        apiKey: redmineClient.apiKey === "" ? "your-api-key" : redmineClient.apiKey

        onErrorOccurred: function (error) {
            messageDialog.text = "Error: " + error
            messageDialog.open()
        }

        onTimeEntryCreated: function (timeEntry) {
            messageDialog.text = "Time entry created successfully"
            messageDialog.open()
        }
    }

    Component.onCompleted: {
        redmineClient.getEnumerations(0)
        redmineClient.getEnumerations(1)
        redmineClient.getEnumerations(2)
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
            activitiesModel.addTimeEntryActivities(value)
        }
    }

    Connections {
        target: redmineClient
        function onIssueReceived(issue) {
            TimeTrackerInstance.addIssueToDatabase(issue)
        }
    }

    Connections {
        target: redmineClient
        function onTimeEntriesReceived(entries) {
            TimeTrackerInstance.addEntriesFromId(entries)
            if (entries.length > 0) {
                redmineClient.getIssue(entries[0].issue.id)
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

        StackView {
            id: stackView
            Layout.fillWidth: true
            Layout.fillHeight: true
            initialItem: trackTimeView
        }
        Item {
            id: footer
            Layout.fillWidth: true
            Layout.minimumHeight: 20
            Layout.maximumHeight: 20

            ConnectionStateLamp {
                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                }
                connectionState: redmineClient.connectionState
            }

            Text {
                id: version
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                }

                color: Material.accentColor
                text: Application.version
            }
        }
    }

    Popup {

        id: busyIndicator
        BusyIndicator {
            running: redmineClient.loading

            onRunningChanged: {
                if (running) {
                    busyIndicator.open()
                } else {
                    busyIndicator.close()
                }
            }
        }
    }

    Component {
        id: trackTimeView
        TrackTimeView {
            timeTracker: timeTrackerInstance
            client: redmineClient
            submitDialog: subDialog
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

    BurgerMenu {

        view: stackView
        componentsToLoad: [trackTimeView, addTimeEntryView, getTimeEntriesView, settingsView]
        buttonTitles: ["Track Time", "Add Time Entry", "Get Time Entries", "Settings"]
    }
}
