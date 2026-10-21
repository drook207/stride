pragma ComponentBehavior: Bound

import QtQuick
import Qt.labs.platform as Platform
import Stride

/*!
 * \brief System tray icon: shows what's tracked, pauses and resumes, starts a pinned issue, and
 * brings the window back (e.g. after it was closed to the tray).
 *
 * Only shown where the platform offers a tray (available); on Linux that needs a tray supporting
 * StatusNotifierItem.
 */
Platform.SystemTrayIcon {
	id: tray

	required property TimeTracker timeTracker
	//! The window to show from the tray.
	required property var window

	function showWindow() {
		tray.window.show()
		tray.window.raise()
		tray.window.requestActivate()
	}

	visible: available
	icon.source: "qrc:/qt/qml/Stride/qml/Assets/Icons/app-icon.svg"
	tooltip: !tray.timeTracker.isTracking ? Application.name
										  : tray.timeTracker.isPaused ? qsTr("#%1 paused at %2").arg(tray.timeTracker.currentIssueId).arg(tray.timeTracker.elapsedTime)
																	  : qsTr("#%1 running: %2").arg(tray.timeTracker.currentIssueId).arg(tray.timeTracker.elapsedTime)

	onActivated: reason => {
		if (reason === Platform.SystemTrayIcon.Trigger) {
			tray.showWindow()
		}
	}

	menu: Platform.Menu {
		id: trayMenu

		Platform.MenuItem {
			text: qsTr("Show %1").arg(Application.name)
			onTriggered: tray.showWindow()
		}
		Platform.MenuSeparator {}
		Platform.MenuItem {
			visible: tray.timeTracker.isTracking
			text: tray.timeTracker.isPaused ? qsTr("Resume #%1").arg(tray.timeTracker.currentIssueId)
											: qsTr("Pause #%1").arg(tray.timeTracker.currentIssueId)
			onTriggered: tray.timeTracker.isPaused ? tray.timeTracker.resumeTracking() : tray.timeTracker.pauseTracking()
		}
		Platform.Menu {
			id: favoritesMenu
			title: qsTr("Start tracking")
			visible: !tray.timeTracker.isTracking && tray.timeTracker.favoriteIssues.length > 0

			Instantiator {
				model: tray.timeTracker.favoriteIssues
				delegate: Platform.MenuItem {
					id: favoriteItem
					required property var modelData
					text: "#" + favoriteItem.modelData.issue_id + (favoriteItem.modelData.subject ? " " + favoriteItem.modelData.subject : "")
					onTriggered: tray.timeTracker.continueTracking(favoriteItem.modelData.issue_id)
				}
				onObjectAdded: (index, object) => favoritesMenu.insertItem(index, object)
				onObjectRemoved: (index, object) => favoritesMenu.removeItem(object)
			}
		}
		Platform.MenuSeparator {}
		Platform.MenuItem {
			text: qsTr("Quit")
			onTriggered: Qt.quit()
		}
	}
}
