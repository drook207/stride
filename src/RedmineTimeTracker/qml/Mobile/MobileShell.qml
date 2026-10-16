import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QRedmine.TimeTracker
import QRedmine.Client

/*!
 * \brief Root layout for the mobile (Android/iOS) UI: MobileHeader, a StackView holding the
 * currently selected view, a collapsible MobileTrackingBar, and a bottom MobileTabBar.
 *
 * Instantiated by Main.qml's Loader when isMobile is true. Mirrors DesktopShell's role and
 * required properties, but replaces the permanent Sidebar with a bottom tab bar and the always-
 * expanded TrackingBar with a collapsible panel, both better suited to a narrow portrait screen.
 */
Item {
    id: docroot

    required property RedmineClient client
    required property TimeTracker timeTracker
    required property var activitiesModel
    required property SubmitDialog submitDialog

    MobileHeader {
        id: header
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }

        connectionState: docroot.client.connectionState
    }

    StackView {
        id: stackView
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: trackingBar.top
        }

        initialItem: recentEntriesView
    }

    MobileTrackingBar {
        id: trackingBar
        anchors {
            left: parent.left
            right: parent.right
            bottom: tabBar.top
        }

        timeTracker: docroot.timeTracker
        client: docroot.client
        submitDialog: docroot.submitDialog
        activitiesModel: docroot.activitiesModel
    }

    MobileTabBar {
        id: tabBar
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        view: stackView
        componentsToLoad: [recentEntriesView, addTimeEntryView, getTimeEntriesView, addIssueView, settingsView]
        buttonTitles: [qsTr("Recent"), qsTr("Add Entry"), qsTr("Search"), qsTr("Add Issue"), qsTr("Settings")]
    }

    Component {
        id: recentEntriesView
        RecentEntriesView {
            timeTracker: docroot.timeTracker
        }
    }

    Component {
        id: addTimeEntryView
        AddTimeEntryView {
            timeTracker: docroot.timeTracker
            client: docroot.client
            activitiesModel: docroot.activitiesModel
        }
    }

    Component {
        id: getTimeEntriesView

        GetTimeEntriesView {
            timeTracker: docroot.timeTracker
            client: docroot.client
        }
    }

    Component {
        id: addIssueView

        AddIssueView {
            client: docroot.client
        }
    }

    Component {
        id: settingsView

        SettingsView {
            timeTracker: docroot.timeTracker
            client: docroot.client
            submitDialog: docroot.submitDialog
        }
    }
}
