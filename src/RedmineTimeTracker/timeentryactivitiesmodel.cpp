#include "timeentryactivitiesmodel.h"
#include "database.h"
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

void TimeEntryActivitiesModel::addTimeEntryActivities(const QJsonArray& activities)
{
	if (!activities.isEmpty()) {
		QVariantList timeEntryActivitiesList;
		for (const QJsonValue& value : std::as_const(activities)) {
			timeEntryActivitiesList.append(value.toObject().toVariantMap());
		}

		setTimeEntryActivities(timeEntryActivitiesList);
		Database::addTimeEntryActivities(activities);
	}
}

}
