#include "reminders.h"
#include "database.h"
#include "logging.h"
#include "timetracker.h"
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSettings>

Q_LOGGING_CATEGORY(lcReminders, "qredmine.reminders")

namespace Stride {

static const QString ENABLED_SETTING = QStringLiteral("reminders/enabled");
static const QString END_OF_DAY_SETTING = QStringLiteral("reminders/endOfDay");
static const QString LONG_TRACKING_SETTING = QStringLiteral("reminders/longTrackingHours");

Reminders::Reminders(QObject* parent)
	: QObject(parent)
{
	QSettings settings;
	mEnabled = settings.value(ENABLED_SETTING, true).toBool();
	mEndOfDay = QTime::fromString(settings.value(END_OF_DAY_SETTING, "17:00").toString(), "HH:mm");
	mLongTrackingHours = settings.value(LONG_TRACKING_SETTING, 3).toInt();

	mTimer.setInterval(60 * 1000);
	connect(&mTimer, &QTimer::timeout, this, [this]() { check(QDateTime::currentDateTime()); });
	mTimer.start();
}

TimeTracker* Reminders::timeTracker() const { return mTimeTracker; }

void Reminders::setTimeTracker(TimeTracker* timeTracker)
{
	if (mTimeTracker == timeTracker) {
		return;
	}
	if (mTimeTracker) {
		disconnect(mTimeTracker, nullptr, this, nullptr);
	}
	mTimeTracker = timeTracker;
	if (mTimeTracker) {
		// A new tracker starts its own count of long stretches.
		connect(mTimeTracker, &TimeTracker::currentIssueChanged, this, [this]() { mLongTrackingReminded = 0; });
		connect(mTimeTracker, &TimeTracker::trackingChanged, this, [this]() {
			if (!mTimeTracker->isTracking()) {
				mLongTrackingReminded = 0;
			}
		});
	}
	emit timeTrackerChanged();
}

QRedmineClient::RedmineClient* Reminders::client() const { return mClient; }

void Reminders::setClient(QRedmineClient::RedmineClient* client)
{
	if (mClient == client) {
		return;
	}
	if (mClient) {
		disconnect(mClient, nullptr, this, nullptr);
	}
	mClient = client;
	if (mClient) {
		connect(mClient, &QRedmineClient::RedmineClient::timeEntryReportReceived, this,
			[this](const QJsonArray& timeEntries, const QString& from, const QString&, int purpose) {
				if (purpose != QRedmineClient::RedmineClient::ReminderReport ||
					from != mAwaitingDay.toString(Qt::ISODate)) {
					return;
				}
				mAwaitingDay = QDate();
				double hours = 0;
				for (const QJsonValue& entry : timeEntries) {
					hours += entry.toObject()["hours"].toDouble();
				}
				QR_DEBUG(lcReminders, QStringLiteral("Redmine has %1 h booked for the day").arg(hours),
					{ { "hours", hours } });
				if (hours < 0.01) {
					remindNothingBooked();
				}
			});
		// Redmine couldn't be asked: go by what's known in the app.
		connect(mClient, &QRedmineClient::RedmineClient::timeEntryReportFailed, this,
			[this](const QString& from, const QString&, int purpose) {
				if (purpose == QRedmineClient::RedmineClient::ReminderReport &&
					from == mAwaitingDay.toString(Qt::ISODate)) {
					mAwaitingDay = QDate();
					remindNothingBooked();
				}
			});
	}
	emit clientChanged();
}

void Reminders::remindNothingBooked()
{
	QR_INFO(lcReminders, QStringLiteral("Reminder shown: nothing booked today"), { { "reminder", "nothingBooked" } });
	emit reminder(tr("Nothing booked today"), tr("No time has been booked for today yet."));
}

bool Reminders::enabled() const { return mEnabled; }

void Reminders::setEnabled(bool enabled)
{
	if (mEnabled == enabled) {
		return;
	}
	mEnabled = enabled;
	QSettings().setValue(ENABLED_SETTING, enabled);
	emit settingsChanged();
}

QString Reminders::endOfDay() const { return mEndOfDay.toString("HH:mm"); }

void Reminders::setEndOfDay(const QString& time)
{
	const QTime parsed = QTime::fromString(time, "HH:mm");
	if (!parsed.isValid() || parsed == mEndOfDay) {
		return;
	}
	mEndOfDay = parsed;
	QSettings().setValue(END_OF_DAY_SETTING, endOfDay());
	emit settingsChanged();
}

int Reminders::longTrackingHours() const { return mLongTrackingHours; }

void Reminders::setLongTrackingHours(int hours)
{
	if (hours < 0 || hours == mLongTrackingHours) {
		return;
	}
	mLongTrackingHours = hours;
	QSettings().setValue(LONG_TRACKING_SETTING, hours);
	emit settingsChanged();
}

void Reminders::check(const QDateTime& now)
{
	if (!mEnabled || !mTimeTracker) {
		return;
	}

	if (mLongTrackingHours > 0 && mTimeTracker->isTracking() && !mTimeTracker->isPaused()) {
		const int stretches = static_cast<int>(mTimeTracker->getElapsedHours() / mLongTrackingHours);
		if (stretches > mLongTrackingReminded) {
			mLongTrackingReminded = stretches;
			QR_INFO(lcReminders,
				QStringLiteral("Reminder shown: the timer has been running for %1 hours")
					.arg(stretches * mLongTrackingHours),
				{ { "reminder", "longTracking" }, { "issueId", mTimeTracker->currentIssueId() },
					{ "hours", stretches * mLongTrackingHours } });
			emit reminder(tr("Timer still running"),
				tr("The timer for #%1 has been running for %2 hours. Still working on it?")
					.arg(mTimeTracker->currentIssueId())
					.arg(stretches * mLongTrackingHours));
		}
	}

	const QDate today = now.date();
	if (today.dayOfWeek() <= 5 && now.time() >= mEndOfDay && mEndOfDayRemindedOn != today &&
		!mTimeTracker->isTracking()) {
		mEndOfDayRemindedOn = today;
		double hours = 0;
		for (const QJsonValue& entry : Database::getEntriesBetween(today, today)) {
			hours += entry.toObject()["hours"].toDouble();
		}
		if (hours >= 0.01) {
			QR_DEBUG(lcReminders, QStringLiteral("End of day: %1 h booked in the app, no reminder").arg(hours),
				{ { "hours", hours } });
			return;
		}
		// Nothing booked in the app; maybe elsewhere (Redmine's web UI, another device).
		if (mClient && !mClient->baseUrl().isEmpty()) {
			QR_DEBUG(lcReminders, QStringLiteral("End of day: nothing booked in the app, asking Redmine"));
			mAwaitingDay = today;
			mClient->getTimeEntryReport(today.toString(Qt::ISODate), today.toString(Qt::ISODate),
				QRedmineClient::RedmineClient::ReminderReport);
		} else {
			remindNothingBooked();
		}
	}
}
}
