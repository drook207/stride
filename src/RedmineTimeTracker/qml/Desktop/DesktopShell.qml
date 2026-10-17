pragma ComponentBehavior: Bound

import QtQuick.Controls.Material
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief Root layout for the desktop UI: Header, collapsible Sidebar, a StackView holding the
 * currently selected view, and the always-visible TrackingBar at the bottom.
 *
 * Instantiated by Main.qml's Loader when isMobile is false. All backend instances are owned by
 * Main.qml and only forwarded here; this component and its children never create their own.
 */
Item {
    id: docroot

    required property RedmineClient client
    required property TimeTracker timeTracker
    //! Reminders whose settings SettingsView offers (see Main.qml).
    required property Reminders reminders
    //! Whether the platform offers a system tray (for SettingsView's close-to-tray option).
    property bool trayAvailable: false
    required property var activitiesModel
    //! Redmine's projects, see Main.qml.
    property var projects: []
    //! The time entry custom field "Special", see Main.qml.
    property var specialField: null
    required property SubmitDialog submitDialog

    Header {
        id: header
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            topMargin: 5
        }

        connectionState: docroot.client.connectionState

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
                timeTracker: docroot.timeTracker
                componentsToLoad: [recentEntriesView, overviewView, addTimeEntryView, getTimeEntriesView, addIssueView, settingsView]
                buttonTitles: [qsTr("Recent Entries"), qsTr("Overview"), qsTr("Add Entry"), qsTr("Search Issue"), qsTr("Add Issue")]
            }

            // Ctrl+1 .. Ctrl+n switch to the sidebar's views, Settings last.
            Instantiator {
                model: sidebar.componentsToLoad.length
                delegate: Shortcut {
                    required property int index
                    sequence: "Ctrl+" + (index + 1)
                    onActivated: sidebar.select(index)
                }
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

            specialField: docroot.specialField
            Layout.fillWidth: true

            timeTracker: docroot.timeTracker
            client: docroot.client
            submitDialog: docroot.submitDialog
            activitiesModel: docroot.activitiesModel
        }
    }

    Component {
        id: recentEntriesView
        RecentEntriesView {
            specialField: docroot.specialField
            timeTracker: docroot.timeTracker
            client: docroot.client
            activitiesModel: docroot.activitiesModel
        }
    }

    Component {
        id: overviewView
        OverviewView {
            timeTracker: docroot.timeTracker
            client: docroot.client
        }
    }

    Component {
        id: addTimeEntryView
        AddTimeEntryView {
            specialField: docroot.specialField
            timeTracker: docroot.timeTracker
            client: docroot.client
            activitiesModel: docroot.activitiesModel
        }
    }

    Component {
        id: getTimeEntriesView

        GetTimeEntriesView {

            specialField: docroot.specialField
            timeTracker: docroot.timeTracker
            client: docroot.client
            activitiesModel: docroot.activitiesModel
        }
    }

    Component {
        id: addIssueView

        AddIssueView {
            client: docroot.client
            projects: docroot.projects
        }
    }

    Component {
        id: settingsView

        SettingsView {
            timeTracker: docroot.timeTracker
            reminders: docroot.reminders
            trayAvailable: docroot.trayAvailable
            client: docroot.client
            submitDialog: docroot.submitDialog
        }
    }
}
