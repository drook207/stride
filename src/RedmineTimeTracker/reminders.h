#pragma once

#include "redmineclient.h"
#include "timetracker.h"
#include <QDate>
#include <QObject>
#include <QPointer>
#include <QTime>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace QRedmineClient {

/*!
 * \brief Reminds the user of time that may be going unbooked or mistracked.
 *
 * Checks once a minute: when the timer has been running for longTrackingHours (again after each
 * further such stretch), and, on workdays from endOfDay on, when nothing has been booked today and
 * nothing is being tracked (once a day). Each emits reminder(); the settings are stored.
 */
class Reminders : public QObject {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(
		QRedmineClient::TimeTracker* timeTracker READ timeTracker WRITE setTimeTracker NOTIFY timeTrackerChanged FINAL)
	//! Asked for the day's time entries before reminding of a day without booked time, so time
	//! booked outside the app (e.g. in Redmine's web UI) counts too. Optional.
	Q_PROPERTY(QRedmineClient::RedmineClient* client READ client WRITE setClient NOTIFY clientChanged FINAL)
	Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged FINAL)
	//! Time of day ("HH:mm") from which a workday without booked time is reminded of.
	Q_PROPERTY(QString endOfDay READ endOfDay WRITE setEndOfDay NOTIFY settingsChanged FINAL)
	//! Running timer stretch reminded of, in hours; 0 for never.
	Q_PROPERTY(int longTrackingHours READ longTrackingHours WRITE setLongTrackingHours NOTIFY settingsChanged FINAL)
public:
	explicit Reminders(QObject* parent = nullptr);

	TimeTracker* timeTracker() const;
	void setTimeTracker(TimeTracker* timeTracker);
	RedmineClient* client() const;
	void setClient(RedmineClient* client);
	bool enabled() const;
	void setEnabled(bool enabled);
	QString endOfDay() const;
	void setEndOfDay(const QString& time);
	int longTrackingHours() const;
	void setLongTrackingHours(int hours);

	//! Checks whether to remind of something at now (called every minute; public for tests).
	void check(const QDateTime& now);

signals:
	void timeTrackerChanged();
	void clientChanged();
	void settingsChanged();
	//! Something to remind the user of, e.g. as a system notification.
	void reminder(const QString& title, const QString& message);

private:
	QPointer<TimeTracker> mTimeTracker;
	QPointer<RedmineClient> mClient;
	//! The day whose time entries were asked for (see client), invalid while not waiting.
	QDate mAwaitingDay;
	//! Reminds of a day without booked time.
	void remindNothingBooked();
	QTimer mTimer;
	bool mEnabled = true;
	QTime mEndOfDay;
	int mLongTrackingHours = 3;
	//! The day nothing booked was reminded of last.
	QDate mEndOfDayRemindedOn;
	//! How many longTrackingHours stretches of the current tracker were reminded of.
	int mLongTrackingReminded = 0;
};
}
