#pragma once

#include <QDate>
#include <QJsonArray>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace Stride {

/*!
 * \brief A week of booked time: hours per day and per project, against a daily target.
 *
 * Counts the user's time entries on Redmine for the week (setRemoteEntries(), from
 * RedmineClient::getTimeEntryReport() for range()) plus the local entries not on Redmine yet. Until
 * Redmine's entries arrive, or without a connection, it counts all local entries instead. Weeks
 * start on Monday; Monday to Friday are workdays with dailyTarget hours each.
 */
class TimeOverview : public QObject {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QDate weekStart READ weekStart NOTIFY changed FINAL)
	//! The week's Sunday.
	Q_PROPERTY(QDate weekEnd READ weekEnd NOTIFY changed FINAL)
	//! ISO 8601 week number.
	Q_PROPERTY(int weekNumber READ weekNumber NOTIFY changed FINAL)
	//! One map per day, Monday first: "date" ("yyyy-MM-dd"), "hours", "target" (0 on weekends),
	//! "isToday", "isFuture".
	Q_PROPERTY(QVariantList days READ days NOTIFY changed FINAL)
	//! One map per project with booked time, most hours first: "name" ("" without one), "hours".
	Q_PROPERTY(QVariantList projects READ projects NOTIFY changed FINAL)
	Q_PROPERTY(double totalHours READ totalHours NOTIFY changed FINAL)
	Q_PROPERTY(double targetHours READ targetHours NOTIFY changed FINAL)
	//! Hours short of the target on the workdays up to today.
	Q_PROPERTY(double missingHours READ missingHours NOTIFY changed FINAL)
	//! Whether the week includes Redmine's entries (rather than only what's known locally).
	Q_PROPERTY(bool includesRedmine READ includesRedmine NOTIFY changed FINAL)
	//! Hours to book per workday; stored in the settings.
	Q_PROPERTY(double dailyTarget READ dailyTarget WRITE setDailyTarget NOTIFY changed FINAL)
public:
	explicit TimeOverview(QObject* parent = nullptr);

	QDate weekStart() const;
	QDate weekEnd() const;
	int weekNumber() const;
	QVariantList days() const;
	QVariantList projects() const;
	double totalHours() const;
	double targetHours() const;
	double missingHours() const;
	bool includesRedmine() const;
	double dailyTarget() const;
	void setDailyTarget(double hours);

	//! The days shown, as "from" and "to" ("yyyy-MM-dd") for RedmineClient::getTimeEntryReport().
	Q_INVOKABLE QVariantMap range() const;

	//! Shows the week containing day; "today" by default.
	Q_INVOKABLE void showWeekOf(const QDate& day = QDate::currentDate());
	Q_INVOKABLE void previousWeek();
	Q_INVOKABLE void nextWeek();

public slots:
	//! Takes Redmine's entries for from .. to (RedmineClient::timeEntryReportReceived); ignored
	//! unless that's the week shown.
	void setRemoteEntries(const QJsonArray& timeEntries, const QString& from, const QString& to);
	//! Counts again, e.g. after local entries changed.
	void refresh();

signals:
	void changed();
	//! The week shown changed; fetch Redmine's entries for range().
	void rangeChanged();

private:
	QDate mWeekStart;
	QJsonArray mRemoteEntries;
	bool mHasRemoteEntries = false;
	QVariantList mDays;
	QVariantList mProjects;
	double mTotalHours = 0;
	double mMissingHours = 0;
	double mDailyTarget = 8;
};
}
