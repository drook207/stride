#include "timeentryactivitiesmodel.h"
#include <QJsonObject>

namespace QRedmineClient {
TimeEntryActivitiesModel::TimeEntryActivitiesModel(QObject* parent)
	: QObject { parent }
{
}

QVariantList TimeEntryActivitiesModel::timeEntryActivities() const { return mTimeEntryActivities; }

void TimeEntryActivitiesModel::setTimeEntryActivities(const QVariantList& newTimeEntryActivities)
{
	if (mTimeEntryActivities == newTimeEntryActivities) {
		return;
	}
	mTimeEntryActivities = newTimeEntryActivities;

	emit timeEntryActivitiesChanged();
}

int TimeEntryActivitiesModel::defaultActivityId() const { return mDefaultActivityId; }

QVariantMap TimeEntryActivitiesModel::projectActivities() const { return mProjectActivities; }

void TimeEntryActivitiesModel::addTimeEntryActivities(const QJsonArray& activities)
{
	if (activities.isEmpty()) {
		return;
	}

	QVariantList active;
	int defaultId = 0;
	for (const QJsonValue& value : std::as_const(activities)) {
		const QJsonObject activity = value.toObject();
		// Inactive activities exist but can't be booked; older Redmine versions omit "active".
		if (!activity.value(QStringLiteral("active")).toBool(true)) {
			continue;
		}
		if (activity.value(QStringLiteral("is_default")).toBool()) {
			defaultId = activity.value(QStringLiteral("id")).toInt();
		}
		active.append(activity.toVariantMap());
	}

	// Set before the list, so its change notification already sees the matching default.
	mDefaultActivityId = defaultId;
	if (mTimeEntryActivities == active) {
		emit timeEntryActivitiesChanged(); // the default may still have changed
		return;
	}
	setTimeEntryActivities(active);
}

void TimeEntryActivitiesModel::setProjectActivities(const QJsonArray& projects)
{
	QVariantMap projectActivities;
	for (const QJsonValue& value : std::as_const(projects)) {
		const QJsonObject project = value.toObject();
		if (!project.contains(QStringLiteral("time_entry_activities"))) {
			continue; // requested without the include, nothing known about its activities
		}
		projectActivities.insert(QString::number(project.value(QStringLiteral("id")).toInt()),
			project.value(QStringLiteral("time_entry_activities")).toArray().toVariantList());
	}

	if (mProjectActivities == projectActivities) {
		return;
	}
	mProjectActivities = projectActivities;
	emit projectActivitiesChanged();
}

}
