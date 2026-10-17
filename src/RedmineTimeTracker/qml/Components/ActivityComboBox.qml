import QtQuick
import QtQuick.Controls

/*!
 * \brief Combo box for a time entry's activity.
 *
 * Offers the activities the issue's project allows (TimeEntryActivitiesModel.projectActivities),
 * or Redmine's active shared activities while the project isn't known. With selectDefault, it
 * preselects Redmine's default activity and keeps an activity the user picked as long as the
 * project allows it, falling back to the default (or the first activity) when switching to a
 * project that doesn't.
 */
ComboBox {
	id: combo

	//! TimeEntryActivitiesModel to take the activities from.
	property var activitiesModel: undefined
	//! Project whose activities are offered, -1 for the shared ones.
	property int projectId: -1
	//! Whether to preselect an activity (see the type description); without it, nothing is
	//! selected until the user or selectActivity() picks one.
	property bool selectDefault: true

	//! The selected activity's id, or 0 if none is selected (Redmine then uses its default).
	readonly property int activityId: combo.currentIndex >= 0 && combo.currentValue !== undefined ? combo.currentValue : 0

	//! The activity the user picked, kept across project changes where allowed.
	property int pickedId: 0

	//! Selects an activity by id; nothing is selected if it isn't offered.
	function selectActivity(id) {
		combo.pickedId = id
		combo.currentIndex = combo.indexOfValue(id)
	}

	function syncSelection() {
		if (!combo.selectDefault) {
			combo.currentIndex = combo.pickedId > 0 ? combo.indexOfValue(combo.pickedId) : -1
			return
		}
		let index = combo.pickedId > 0 ? combo.indexOfValue(combo.pickedId) : -1
		if (index < 0 && combo.activitiesModel) {
			index = combo.indexOfValue(combo.activitiesModel.defaultActivityId)
		}
		combo.currentIndex = index >= 0 ? index : (combo.count > 0 ? 0 : -1)
	}

	model: {
		if (!combo.activitiesModel) {
			return []
		}
		const allowed = combo.activitiesModel.projectActivities[combo.projectId]
		return combo.projectId > 0 && allowed !== undefined ? allowed : combo.activitiesModel.timeEntryActivities
	}
	textRole: "name"
	valueRole: "id"

	implicitHeight: Metrics.controlHeight
	topInset: 0
	bottomInset: 0

	// After the model is replaced, count and indexOfValue() only reflect it once the change is
	// processed, hence the deferred sync.
	onModelChanged: Qt.callLater(combo.syncSelection)
	Component.onCompleted: combo.syncSelection()
	onActivated: combo.pickedId = combo.currentValue

	Connections {
		target: combo.activitiesModel
		function onTimeEntryActivitiesChanged() {
			Qt.callLater(combo.syncSelection)
		}
	}
}
