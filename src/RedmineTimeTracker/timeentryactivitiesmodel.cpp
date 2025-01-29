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

void TimeEntryActivitiesModel::addTimeEntryActivities(QJsonArray& ativities)
{
	if (!ativities.isEmpty()) {
		QVariantList timeEntryActivities;
		for (const QJsonValue& value : std::as_const(ativities)) {
			timeEntryActivities.append(value.toObject().toVariantMap());
		}

		setTimeEntryActivities(timeEntryActivities);
	}
}

}
