#pragma once

#include <QJsonArray>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>

namespace Stride {
/*!
 * \brief The TimeEntryActivitiesModel class holds the time entry activities a time entry can be
 * booked with: Redmine's active shared activities, its default activity, and the activities each
 * project allows (projects can deactivate shared activities for themselves).
 */
class TimeEntryActivitiesModel : public QObject {
	Q_OBJECT
	QML_ELEMENT
	//! Active shared activities ({id, name, ...}); inactive ones are left out.
	Q_PROPERTY(QVariantList timeEntryActivities READ timeEntryActivities WRITE setTimeEntryActivities NOTIFY
			timeEntryActivitiesChanged FINAL)
	//! Id of Redmine's default activity, or 0 if there is none.
	Q_PROPERTY(int defaultActivityId READ defaultActivityId NOTIFY timeEntryActivitiesChanged FINAL)
	//! Activities each project allows ({id, name}), keyed by project id; projects not in here
	//! (e.g. while the project list isn't loaded) allow the shared activities.
	Q_PROPERTY(QVariantMap projectActivities READ projectActivities NOTIFY projectActivitiesChanged FINAL)
public:
	explicit TimeEntryActivitiesModel(QObject* parent = nullptr);

	QVariantList timeEntryActivities() const;
	void setTimeEntryActivities(const QVariantList& newTimeEntryActivities);

	int defaultActivityId() const;

	QVariantMap projectActivities() const;

public slots:
	/*!
	 * \brief addTimeEntryActivities Takes Redmine's shared activities
	 * (RedmineClient::timeEntryActivitiesReceived), keeping only the active ones
	 */
	void addTimeEntryActivities(const QJsonArray& activities);

	/*!
	 * \brief setProjectActivities Takes the activities each project allows from Redmine's project
	 * list (RedmineClient::projectsReceived, requested with the time_entry_activities include)
	 */
	void setProjectActivities(const QJsonArray& projects);

signals:
	void timeEntryActivitiesChanged();
	void projectActivitiesChanged();

private:
	QVariantList mTimeEntryActivities;
	int mDefaultActivityId = 0;
	QVariantMap mProjectActivities;
};
}
