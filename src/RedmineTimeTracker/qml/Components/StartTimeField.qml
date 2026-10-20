import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QRedmine.TimeTracker

/*!
 * \brief Start time of the tracked time (TrackingBar/MobileTrackingBar): shows TimeTracker::startedAt and
 * lets the user correct it, e.g. after forgetting to start the tracker.
 *
 * Shows nothing while nothing is tracked, and is locked while the tracker waits for an answer about
 * the time away. A picked time is meant as the occurrence nearest to the current start (within 12
 * hours of it); if that lies in the future or TimeTracker::setStartedAt() refuses it, the field shows
 * the tracker's start again and a short hint explains why.
 */
TimePickerField {
	id: field

	required property TimeTracker timeTracker

	//! Whether the tracker waits for an answer about the time away (see TimeTracker::interruptedAt).
	readonly property bool interruptionPending: {
		const at = field.timeTracker.interruptedAt
		return at instanceof Date && !isNaN(at.getTime())
	}

	//! Shows the tracker's start, or nothing while nothing is tracked.
	function sync() {
		const started = field.timeTracker.startedAt
		const valid = field.timeTracker.isTracking && started instanceof Date && !isNaN(started.getTime())
		field.hasTime = valid
		if (valid) {
			field.hours = started.getHours()
			field.minutes = started.getMinutes()
		}
	}

	enabled: field.timeTracker.isTracking
	pickable: !field.interruptionPending
	opacity: field.pickable ? 1 : 0.5
	Component.onCompleted: field.sync()

	onTimePicked: {
		const current = field.timeTracker.startedAt
		if (!(current instanceof Date) || isNaN(current.getTime())) {
			field.sync();
			return;
		}
		// The wheels start at the current minute floored to the 5-minute step: unchanged means no edit.
		if (field.hours === current.getHours() && field.minutes === Math.floor(current.getMinutes() / 5) * 5) {
			field.sync();
			return;
		}
		// The occurrence of the picked clock time nearest to the current start (within 12 hours of it).
		let start = null;
		for (let offset = -1; offset <= 1; ++offset) {
			const candidate = new Date(current.getTime());
			candidate.setDate(candidate.getDate() + offset);
			candidate.setHours(field.hours, field.minutes, 0, 0);
			if (start === null || Math.abs(candidate.getTime() - current.getTime()) < Math.abs(start.getTime() - current.getTime())) {
				start = candidate;
			}
		}
		if (start.getTime() > Date.now() || !field.timeTracker.setStartedAt(start)) {
			field.sync();
			hint.open();
		}
	}
	// The clear button has no meaning here: the start can only be changed, not removed.
	onHasTimeChanged: {
		if (!field.hasTime) {
			field.sync();
		}
	}

	Connections {
		target: field.timeTracker
		function onStartedAtChanged() {
			field.sync();
		}
		function onIsTrackingChanged() {
			field.sync();
		}
	}

	ToolTip {
		id: hint
		timeout: 4000
		text: qsTr("The start can't be in the future, more than 24 hours ago, or after the paused time.")
	}

	ToolTip {
		visible: field.interruptionPending && field.hovered
		delay: 500
		text: qsTr("Answer the question about the time away first.")
	}
}
