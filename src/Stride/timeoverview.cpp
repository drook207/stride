#include "timeoverview.h"
#include "database.h"
#include <QHash>
#include <QJsonObject>
#include <QSettings>
#include <algorithm>

namespace Stride {

static const QString DAILY_TARGET_SETTING = QStringLiteral("overview/dailyTargetHours");

TimeOverview::TimeOverview(QObject* parent)
	: QObject(parent)
	, mDailyTarget(QSettings().value(DAILY_TARGET_SETTING, 8.0).toDouble())
{
	showWeekOf(QDate::currentDate());
}

QDate TimeOverview::weekStart() const { return mWeekStart; }

QDate TimeOverview::weekEnd() const { return mWeekStart.addDays(6); }

int TimeOverview::weekNumber() const { return mWeekStart.weekNumber(); }

QVariantList TimeOverview::days() const { return mDays; }

QVariantList TimeOverview::projects() const { return mProjects; }

double TimeOverview::totalHours() const { return mTotalHours; }

double TimeOverview::targetHours() const { return mDailyTarget * 5; }

double TimeOverview::missingHours() const { return mMissingHours; }

bool TimeOverview::includesRedmine() const { return mHasRemoteEntries; }

double TimeOverview::dailyTarget() const { return mDailyTarget; }

void TimeOverview::setDailyTarget(double hours)
{
	if (hours < 0 || qFuzzyCompare(hours + 1, mDailyTarget + 1)) {
		return;
	}
	mDailyTarget = hours;
	QSettings().setValue(DAILY_TARGET_SETTING, hours);
	refresh();
}

QVariantMap TimeOverview::range() const
{
	return { { "from", mWeekStart.toString(Qt::ISODate) }, { "to", mWeekStart.addDays(6).toString(Qt::ISODate) } };
}

void TimeOverview::showWeekOf(const QDate& day)
{
	const QDate weekStart = day.addDays(1 - day.dayOfWeek());
	if (weekStart == mWeekStart) {
		return;
	}
	mWeekStart = weekStart;
	mRemoteEntries = QJsonArray();
	mHasRemoteEntries = false;
	refresh();
	emit rangeChanged();
}

void TimeOverview::previousWeek() { showWeekOf(mWeekStart.addDays(-7)); }

void TimeOverview::nextWeek() { showWeekOf(mWeekStart.addDays(7)); }

void TimeOverview::setRemoteEntries(const QJsonArray& timeEntries, const QString& from, const QString& to)
{
	const QVariantMap shown = range();
	if (from != shown["from"].toString() || to != shown["to"].toString()) {
		return; // for a week no longer shown
	}
	mRemoteEntries = timeEntries;
	mHasRemoteEntries = true;
	refresh();
}

void TimeOverview::refresh()
{
	const QDate weekEnd = mWeekStart.addDays(6);
	QHash<QDate, double> hoursPerDay;
	QHash<QString, double> hoursPerProject;
	auto count = [&](const QDate& day, double hours, const QString& project) {
		if (day < mWeekStart || day > weekEnd) {
			return;
		}
		hoursPerDay[day] += hours;
		hoursPerProject[project] += hours;
	};

	if (mHasRemoteEntries) {
		for (const QJsonValue& value : std::as_const(mRemoteEntries)) {
			const QJsonObject entry = value.toObject();
			count(QDate::fromString(entry["spent_on"].toString(), Qt::ISODate), entry["hours"].toDouble(),
				entry["project"].toObject()["name"].toString());
		}
	}
	for (const QJsonValue& value : Database::getEntriesBetween(mWeekStart, weekEnd)) {
		const QJsonObject entry = value.toObject();
		// With Redmine's entries at hand, only what isn't there yet; unconfirmed entries may be.
		const int status = entry["sync_status"].toInt();
		if (mHasRemoteEntries && status != Database::Pending && status != Database::Rejected) {
			continue;
		}
		count(QDate::fromString(entry["spent_on"].toString().left(10), Qt::ISODate), entry["hours"].toDouble(),
			entry["project_name"].toString());
	}

	const QDate today = QDate::currentDate();
	mDays.clear();
	mTotalHours = 0;
	mMissingHours = 0;
	for (int i = 0; i < 7; ++i) {
		const QDate day = mWeekStart.addDays(i);
		const double hours = hoursPerDay.value(day);
		const double target = day.dayOfWeek() <= 5 ? mDailyTarget : 0;
		mDays.append(QVariantMap { { "date", day.toString(Qt::ISODate) }, { "hours", hours }, { "target", target },
			{ "isToday", day == today }, { "isFuture", day > today } });
		mTotalHours += hours;
		if (day <= today) {
			mMissingHours += std::max(0.0, target - hours);
		}
	}

	QList<QPair<QString, double>> projects(hoursPerProject.keyValueBegin(), hoursPerProject.keyValueEnd());
	std::sort(projects.begin(), projects.end(), [](const auto& a, const auto& b) {
		return a.second != b.second ? a.second > b.second : a.first.localeAwareCompare(b.first) < 0;
	});
	mProjects.clear();
	for (const auto& [name, hours] : std::as_const(projects)) {
		mProjects.append(QVariantMap { { "name", name }, { "hours", hours } });
	}
	emit changed();
}
}
