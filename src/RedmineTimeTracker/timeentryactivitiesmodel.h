#pragma once

#include <QJsonArray>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

namespace QRedmineClient {
class TimeEntryActivitiesModel : public QObject {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QVariantList timeEntryActivities READ timeEntryActivities WRITE setTimeEntryActivities NOTIFY
			timeEntryActivitiesChanged FINAL)
public:
	explicit TimeEntryActivitiesModel(QObject* parent = nullptr);

	QVariantList timeEntryActivities() const;
	void setTimeEntryActivities(const QVariantList& newTimeEntryActivities);

public slots:
	void addTimeEntryActivities(QJsonArray& ativities);

signals:
	void timeEntryActivitiesChanged();

private:
	QVariantList mTimeEntryActivities;
};
}
