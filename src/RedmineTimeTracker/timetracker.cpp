// timetracker.cpp
#include "timetracker.h"
#include "database.h"
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
	if (!Database::initialize()) {
		qCWarning(lcTimeTracker) << "Could not initialize database";
	}
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

int TimeTracker::addRecentEntry(const QVariantMap& entry)
{
	const auto hours = entry["hours"].toDouble();
	const auto comment = entry["comments"].toString();
	const auto issueId = entry["issueId"].toInt();
	const auto spentOn = entry["date"].toString();
	QDateTime spentOnAsDateTime =
		spentOn.isEmpty() ? QDateTime::currentDateTime() : QDateTime::fromString(spentOn, "yyyy-MM-dd");

	const int localId = Database::addTimeEntry(issueId, hours, comment, spentOnAsDateTime);
	if (localId < 0) {
		qWarning(lcTimeTracker) << "Could not save to database!";
		return -1;
	}

	// Mirror the field names Database::getRecentEntries() produces (issue_id/spent_on, not
	// issueId/date), since this entry is displayed immediately without waiting for a reload.
	QVariantMap canonicalEntry;
	canonicalEntry["id"] = localId;
	canonicalEntry["issue_id"] = issueId;
	canonicalEntry["hours"] = hours;
	canonicalEntry["comments"] = comment;
	canonicalEntry["spent_on"] = spentOnAsDateTime.toString(Qt::ISODate);
	canonicalEntry["sync_status"] = 0;
	canonicalEntry["redmine_id"] = 0;

	mRecentEntries.prepend(canonicalEntry);

	// Keep only last 50 entries
	while (mRecentEntries.size() > 50) {
		mRecentEntries.removeLast();
	}

	emit recentEntriesChanged();
	return localId;
}

void TimeTracker::addEntriesFromId(const QJsonArray& entries)
{
	mEntriesFromId.clear();
	for (const QJsonValue& value : std::as_const(entries)) {
		mEntriesFromId.append(value.toObject().toVariantMap());
	}

	emit entriesFromIdChanged();
}

void TimeTracker::addIssueToDatabase(const QJsonObject& issue)
{
	const auto id = issue["id"].toInt(-1);
	const auto subject = issue["subject"].toString();
	const auto project = issue["project"]["id"].toInt();
	const auto status = issue["status"]["id"].toInt();
	const auto description = issue["description"].toString();
	const auto createdOn = issue["created_on"].toVariant().toDateTime();
	const auto updatedOn = issue["updated_on"].toVariant().toDateTime();

	if (!Database::addIssue(id, subject, project, status, description, createdOn, updatedOn)) {
		qWarning() << "Could not add issue  " << issue;
	} else {
		emit issueAddedOrUpdated();
	}
}

void TimeTracker::loadRecentEntries()
{
	auto entries = Database::getRecentEntries();
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

void TimeTracker::updateRecentEntry(int localId, const QVariantMap& changes)
{
	const QDateTime spentOn(QDate::fromString(changes["spent_on"].toString(), Qt::ISODate), QTime(0, 0));
	const int changed =
		Database::updateTimeEntry(localId, changes["hours"].toDouble(), changes["comments"].toString(), spentOn);
	if (changed < 0) {
		qCWarning(lcTimeTracker) << "Could not update local time entry" << localId;
	} else if (changed > 0) {
		loadRecentEntries();
	}
}

int TimeTracker::stageRedmineUpdate(int redmineId, const QVariantMap& changes)
{
	QVariantMap staged = changes;
	staged["redmine_id"] = redmineId;
	mPendingRedmineUpdates.insert(++mLastRedmineUpdateTag, staged);
	return mLastRedmineUpdateTag;
}

void TimeTracker::applyRedmineUpdate(int redmineId, int tag)
{
	const QVariantMap changes = mPendingRedmineUpdates.take(tag);
	if (changes.isEmpty() || changes["redmine_id"].toInt() != redmineId) {
		return;
	}
	const double hours = changes["hours"].toDouble();
	const QString comments = changes["comments"].toString();
	const QString spentOn = changes["spent_on"].toString();

	const int changed = Database::updateTimeEntriesByRedmineId(
		redmineId, hours, comments, QDateTime(QDate::fromString(spentOn, Qt::ISODate), QTime(0, 0)));
	if (changed < 0) {
		qCWarning(lcTimeTracker) << "Could not update local copies of Redmine time entry" << redmineId;
	} else if (changed > 0) {
		loadRecentEntries();
	}

	// Mirror the change in the Search Issue results, so they don't need to be fetched again.
	bool resultsChanged = false;
	for (auto& value : mEntriesFromId) {
		QVariantMap entry = value.toMap();
		if (entry["id"].toInt() != redmineId) {
			continue;
		}
		entry["hours"] = hours;
		entry["comments"] = comments;
		entry["spent_on"] = spentOn;
		if (changes.contains("activity_id")) {
			QVariantMap activity = entry["activity"].toMap();
			activity["id"] = changes["activity_id"];
			activity["name"] = changes["activity_name"];
			entry["activity"] = activity;
		}
		value = entry;
		resultsChanged = true;
	}
	if (resultsChanged) {
		emit entriesFromIdChanged();
	}
}

void TimeTracker::discardRedmineUpdate(int redmineId, int tag)
{
	if (mPendingRedmineUpdates.value(tag)["redmine_id"].toInt() == redmineId) {
		mPendingRedmineUpdates.remove(tag);
	}
}

void TimeTracker::linkCreatedTimeEntry(const QJsonObject& timeEntry, int localId)
{
	const int redmineId = timeEntry["id"].toInt(-1);
	if (redmineId < 0 || localId <= 0) {
		return;
	}
	if (Database::updateSyncStatus(localId, redmineId)) {
		loadRecentEntries();
	}
}

void TimeTracker::updateElapsedTime() { emit elapsedTimeChanged(); }
}
