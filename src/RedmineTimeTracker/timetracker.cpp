// timetracker.cpp
#include "timetracker.h"
#include "timetrackerdatabase.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcTimeTracker, "qredmine.timetracker")

namespace QRedmineClient {
TimeTracker::TimeTracker(QObject* parent)
	: QObject(parent)
	, mCurrentIssueId(0)
	, mIsPaused(false)
	, mTotalElapsedTime(0)
{
	connect(&mTimer, &QTimer::timeout, this, &TimeTracker::updateElapsedTime);
	mTimer.setInterval(1000); // Update every second
	TimeEntryDatabase::initialize();
	loadRecentEntries();
}

bool TimeTracker::isTracking() const { return mTimer.isActive() || mIsPaused; }

bool TimeTracker::isPaused() const { return mIsPaused; }

int TimeTracker::currentIssueId() const { return mCurrentIssueId; }

QString TimeTracker::elapsedTime() const
{
	qint64 elapsed = mTotalElapsedTime;

	if (mTimer.isActive()) {
		elapsed += mStartTime.secsTo(QDateTime::currentDateTime());
	}

	int hours = elapsed / 3600;
	int minutes = (elapsed % 3600) / 60;
	int seconds = elapsed % 60;

	return QString("%1:%2:%3")
		.arg(hours, 2, 10, QLatin1Char('0'))
		.arg(minutes, 2, 10, QLatin1Char('0'))
		.arg(seconds, 2, 10, QLatin1Char('0'));
}

QVariantList TimeTracker::recentEntries() const { return mRecentEntries; }

double TimeTracker::getElapsedHours() const
{
	qint64 elapsed = mTotalElapsedTime;

	if (mTimer.isActive()) {
		elapsed += mStartTime.secsTo(QDateTime::currentDateTime());
	}

	return elapsed / 3600.0;
}

void TimeTracker::startTracking(int issueId)
{
	if (mTimer.isActive()) {
		stopTracking();
	}

	mCurrentIssueId = issueId;
	mStartTime = QDateTime::currentDateTime();
	mTotalElapsedTime = 0;
	mIsPaused = false;
	mTimer.start();

	emit trackingChanged();
	emit currentIssueChanged();
	emit pausedChanged();
}

void TimeTracker::stopTracking()
{
	if (mTimer.isActive() || mIsPaused) {
		if (!mIsPaused) {
			mTotalElapsedTime += mStartTime.secsTo(QDateTime::currentDateTime());
		}
		mTimer.stop();
		mIsPaused = false;
		emit trackingChanged();
		emit pausedChanged();
	}
}

void TimeTracker::pauseTracking()
{
	if (mTimer.isActive() && !mIsPaused) {
		mTotalElapsedTime += mStartTime.secsTo(QDateTime::currentDateTime());
		mTimer.stop();
		mIsPaused = true;
		emit pausedChanged();
		emit trackingChanged();
	}
}

void TimeTracker::resumeTracking()
{
	if (!mTimer.isActive() && mIsPaused) {
		mStartTime = QDateTime::currentDateTime();
		mTimer.start();
		mIsPaused = false;
		emit pausedChanged();
		emit trackingChanged();
	}
}

void TimeTracker::addRecentEntry(const QVariantMap& entry)
{
	mRecentEntries.prepend(entry);

	// Keep only last 50 entries
	while (mRecentEntries.size() > 50) {
		mRecentEntries.removeLast();
	}

	const auto hours = entry["hours"].toDouble();
	const auto comment = entry["comments"].toString();
	const auto issueId = entry["issueId"].toInt();
	const bool result = TimeEntryDatabase::addTimeEntry(issueId, hours, comment);
	if (!result) {
		qWarning(lcTimeTracker) << "Could not save to database!";
		return;
	}
	emit entriesFromIdChanged();
}

void TimeTracker::addEntriesFromId(const QJsonArray& entries)
{
	if (!entries.isEmpty()) {
		mEntriesFromId.clear();
		for (const QJsonValue& value : std::as_const(entries)) {
			mEntriesFromId.append(value.toObject().toVariantMap());
		}

		emit entriesFromIdChanged();
	}
}

void TimeTracker::loadRecentEntries()
{
	auto entries = TimeEntryDatabase::getRecentEntries();
	if (!entries.isEmpty()) {
		mRecentEntries.clear();
		for (const QJsonValue& value : std::as_const(entries)) {
			mRecentEntries.append(value.toObject().toVariantMap());
		}

		emit recentEntriesChanged();
	}
}

QVariantList TimeTracker::entriesFromId() const { return mEntriesFromId; }

void TimeTracker::setEntriesFromId(const QVariantList& newEntriesFromId)
{
	if (mEntriesFromId == newEntriesFromId) {
		return;
	}
	mEntriesFromId = newEntriesFromId;
	emit entriesFromIdChanged();
}

void TimeTracker::updateElapsedTime() { emit elapsedTimeChanged(); }
}
