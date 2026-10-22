// timetracker.cpp
#include "timetracker.h"
#include "database.h"
#include "logging.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSet>
#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcTimeTracker, "qredmine.timetracker")
// What the user entered (e.g. comments), only logged at the "debug" log level
Q_LOGGING_CATEGORY(lcTimeTrackerContent, "qredmine.content")

namespace Stride {
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
	restoreTrackingState();
	loadFavoriteIssues();
	// A pinned issue's subject and project may only be known once the issue is cached.
	connect(this, &TimeTracker::issueCacheChanged, this, &TimeTracker::loadFavoriteIssues);
}

bool TimeTracker::continueTracking(int issueId, const QString& comment, int activityId)
{
	if (isTracking() || issueId <= 0) {
		return false;
	}
	setTrackingComment(comment);
	setTrackingActivityId(activityId);
	startTracking(issueId);
	return true;
}

QVariantList TimeTracker::favoriteIssues() const { return mFavoriteIssues; }

bool TimeTracker::isFavoriteIssue(int issueId) const
{
	return std::any_of(mFavoriteIssues.cbegin(), mFavoriteIssues.cend(),
		[issueId](const QVariant& favorite) { return favorite.toMap()["issue_id"].toInt() == issueId; });
}

void TimeTracker::toggleFavoriteIssue(int issueId)
{
	if (issueId <= 0) {
		return;
	}
	if (isFavoriteIssue(issueId) ? Database::removeFavoriteIssue(issueId) : Database::addFavoriteIssue(issueId)) {
		loadFavoriteIssues();
	}
}

void TimeTracker::loadFavoriteIssues()
{
	const QVariantList favorites = Database::getFavoriteIssues();
	if (favorites != mFavoriteIssues) {
		mFavoriteIssues = favorites;
		emit favoriteIssuesChanged();
	}
}

TimeTracker::~TimeTracker()
{
	if (isTracking()) {
		Database::touchTrackingState();
	}
}

QString TimeTracker::trackingComment() const { return mTrackingComment; }

void TimeTracker::setTrackingComment(const QString& comment)
{
	if (mTrackingComment == comment) {
		return;
	}
	mTrackingComment = comment;
	emit trackingDetailsChanged();
	if (isTracking()) {
		saveTrackingState();
	}
}

int TimeTracker::trackingActivityId() const { return mTrackingActivityId; }

void TimeTracker::setTrackingActivityId(int activityId)
{
	if (mTrackingActivityId == activityId) {
		return;
	}
	mTrackingActivityId = activityId;
	emit trackingDetailsChanged();
	if (isTracking()) {
		saveTrackingState();
	}
}

QDateTime TimeTracker::interruptedAt() const { return mInterruptedAt; }

bool TimeTracker::interruptedBySleep() const { return mInterruptedBySleep; }

QDateTime TimeTracker::startedAt() const { return mStartedAt; }

//! The longest the start may lie back, see TimeTracker::setStartedAt().
static constexpr int MAX_START_AGE_SECONDS = 24 * 60 * 60;

bool TimeTracker::setStartedAt(const QDateTime& startedAt)
{
	const QDateTime now = QDateTime::currentDateTime();
	const char* reason = nullptr;
	const qint64 elapsed = elapsedSeconds();
	// Earlier by N seconds adds N seconds.
	const qint64 shift = startedAt.isValid() ? startedAt.secsTo(mStartedAt) : 0;
	if (!isTracking()) {
		reason = "not tracking";
	} else if (mInterruptedAt.isValid()) {
		reason = "the time away is undecided";
	} else if (!startedAt.isValid()) {
		reason = "invalid date";
	} else if (startedAt > now) {
		reason = "in the future";
	} else if (startedAt.secsTo(now) > MAX_START_AGE_SECONDS) {
		reason = "more than 24 hours ago";
	} else if (elapsed + shift < 0) {
		reason = "more time would be taken off than tracked";
	}
	if (reason) {
		QR_DEBUG(lcTimeTracker, QStringLiteral("Rejected a new start time: %1").arg(QLatin1String(reason)),
			{ { "issueId", mCurrentIssueId }, { "startedAt", startedAt.toString(Qt::ISODate) } });
		return false;
	}
	const QDateTime oldStart = mStartedAt;
	mStartedAt = startedAt;
	mTotalElapsedTime += shift;
	saveTrackingState();
	QR_INFO(lcTimeTracker,
		QStringLiteral("Changed the start of issue %1 from %2 to %3")
			.arg(mCurrentIssueId)
			.arg(oldStart.toString(Qt::ISODate), startedAt.toString(Qt::ISODate)),
		{ { "issueId", mCurrentIssueId }, { "oldStartedAt", oldStart.toString(Qt::ISODate) },
			{ "startedAt", startedAt.toString(Qt::ISODate) }, { "elapsedSeconds", elapsed + shift } });
	emit startedAtChanged();
	emit elapsedTimeChanged();
	return true;
}

void TimeTracker::clearTracking()
{
	const bool wasTracking = isTracking();
	stopTracking();
	const bool hadIssue = mCurrentIssueId != 0;
	const bool hadDetails = !mTrackingComment.isEmpty();
	const bool hadStart = mStartedAt.isValid();
	mCurrentIssueId = 0;
	mTotalElapsedTime = 0;
	mTrackingComment.clear();
	mStartedAt = QDateTime();
	mStartTime = QDateTime();
	if (!wasTracking) {
		Database::clearTrackingState(); // stopTracking() did it otherwise
	}
	QR_INFO(lcTimeTracker, QStringLiteral("Cleared the tracker"), { { "hadIssue", hadIssue } });
	if (hadIssue) {
		emit currentIssueChanged();
	}
	if (hadDetails) {
		emit trackingDetailsChanged();
	}
	if (hadStart) {
		emit startedAtChanged();
	}
	emit elapsedTimeChanged();
	emit trackingCleared();
}

qint64 TimeTracker::elapsedSeconds() const
{
	return mTotalElapsedTime + (mTimer.isActive() ? mStartTime.secsTo(QDateTime::currentDateTime()) : 0);
}

void TimeTracker::saveTrackingState()
{
	if (!isTracking()) {
		Database::clearTrackingState();
		return;
	}
	Database::saveTrackingState(
		{ { "issue_id", mCurrentIssueId }, { "started_at", mTimer.isActive() ? mStartTime : QDateTime() },
			{ "elapsed_seconds", mTotalElapsedTime }, { "comment", mTrackingComment },
			{ "activity_id", mTrackingActivityId }, { "tracking_started_at", mStartedAt } });
	mSecondsSinceTouch = 0;
}

void TimeTracker::restoreTrackingState()
{
	const QVariantMap state = Database::loadTrackingState();
	if (state.isEmpty()) {
		return;
	}
	mCurrentIssueId = state["issue_id"].toInt();
	mTotalElapsedTime = state["elapsed_seconds"].toLongLong();
	mTrackingComment = state["comment"].toString();
	mTrackingActivityId = state["activity_id"].toInt();
	mStartTime = state["started_at"].toDateTime();
	mIsPaused = !mStartTime.isValid();
	mStartedAt = state["tracking_started_at"].toDateTime();
	const QDateTime now = QDateTime::currentDateTime();
	const QDateTime lastSeen = state["last_seen"].toDateTime();
	if (!mIsPaused) {
		mTimer.start();
		if (lastSeen.isValid() && lastSeen.secsTo(now) > INTERRUPTION_THRESHOLD_SECONDS) {
			mInterruptedAt = lastSeen;
			QR_INFO(lcTimeTracker,
				QStringLiteral("The app was gone since %1 while tracking").arg(lastSeen.toString(Qt::ISODate)),
				{ { "issueId", mCurrentIssueId }, { "lastSeen", lastSeen.toString(Qt::ISODate) } });
		}
	}
	if (!mStartedAt.isValid()) {
		// Saved before the start was kept: what's tracked so far ends now, or where the app was last
		// seen if the time away is still to be decided.
		const QDateTime end = mInterruptedAt.isValid() ? mInterruptedAt : now;
		const qint64 segment = mIsPaused ? 0 : std::max<qint64>(0, mStartTime.secsTo(end));
		mStartedAt = end.addSecs(-(mTotalElapsedTime + segment));
	}
	QR_INFO(lcTimeTracker,
		QStringLiteral("Restored the tracker for issue %1 (%2)")
			.arg(mCurrentIssueId)
			.arg(mIsPaused ? QStringLiteral("paused") : QStringLiteral("running")),
		{ { "issueId", mCurrentIssueId }, { "paused", mIsPaused }, { "elapsedSeconds", mTotalElapsedTime },
			{ "interruptedAt", mInterruptedAt.isValid() ? mInterruptedAt.toString(Qt::ISODate) : QString() } });
	QR_DEBUG(lcTimeTrackerContent, QStringLiteral("Restored tracker comment"), { { "comment", mTrackingComment } });
}

void TimeTracker::resolveInterruption(bool countTimeAway)
{
	if (!mInterruptedAt.isValid()) {
		return;
	}
	QR_INFO(lcTimeTracker,
		QStringLiteral("Interruption since %1: the user chose to %2 the time away")
			.arg(
				mInterruptedAt.toString(Qt::ISODate), countTimeAway ? QStringLiteral("count") : QStringLiteral("skip")),
		{ { "countTimeAway", countTimeAway }, { "interruptedAt", mInterruptedAt.toString(Qt::ISODate) },
			{ "bySleep", mInterruptedBySleep } });
	if (!countTimeAway && mTimer.isActive()) {
		// Stop the clock where it was when the app was last seen...
		mTotalElapsedTime += std::max<qint64>(0, mStartTime.secsTo(mInterruptedAt));
		if (mInterruptedBySleep) {
			// ...and, after sleep, go on from waking up: the user may have worked a while before
			// answering.
			mStartTime = mResumedAt.isValid() ? mResumedAt : QDateTime::currentDateTime();
		} else {
			mTimer.stop();
			mIsPaused = true;
			emit pausedChanged();
			emit trackingChanged();
		}
		emit elapsedTimeChanged();
	}
	mInterruptedAt = QDateTime();
	mInterruptedBySleep = false;
	emit interruptedAtChanged();
	saveTrackingState();
}

bool TimeTracker::isTracking() const { return mTimer.isActive() || mIsPaused; }

bool TimeTracker::isPaused() const { return mIsPaused; }

int TimeTracker::currentIssueId() const { return mCurrentIssueId; }

QString TimeTracker::elapsedTime() const
{
	const qint64 elapsed = elapsedSeconds();

	int hours = elapsed / 3600;
	int minutes = (elapsed % 3600) / 60;
	int seconds = elapsed % 60;

	return QString("%1:%2:%3")
		.arg(hours, 2, 10, QLatin1Char('0'))
		.arg(minutes, 2, 10, QLatin1Char('0'))
		.arg(seconds, 2, 10, QLatin1Char('0'));
}

QVariantList TimeTracker::recentEntries() const { return mRecentEntries; }

double TimeTracker::getElapsedHours() const { return elapsedSeconds() / 3600.0; }

void TimeTracker::startTracking(int issueId)
{
	if (mTimer.isActive()) {
		stopTracking();
	}

	mCurrentIssueId = issueId;
	mStartTime = QDateTime::currentDateTime();
	mStartedAt = mStartTime;
	mTotalElapsedTime = 0;
	mIsPaused = false;
	mLastTick = QDateTime();
	mTimer.start();
	saveTrackingState();
	QR_INFO(lcTimeTracker, QStringLiteral("Started tracking issue %1").arg(issueId), { { "issueId", issueId } });

	emit trackingChanged();
	emit currentIssueChanged();
	emit pausedChanged();
	emit startedAtChanged();
}

void TimeTracker::stopTracking()
{
	if (mTimer.isActive() || mIsPaused) {
		if (!mIsPaused) {
			mTotalElapsedTime += mStartTime.secsTo(QDateTime::currentDateTime());
		}
		mTimer.stop();
		mIsPaused = false;
		saveTrackingState();
		QR_INFO(lcTimeTracker,
			QStringLiteral("Stopped tracking issue %1 after %2 s").arg(mCurrentIssueId).arg(mTotalElapsedTime),
			{ { "issueId", mCurrentIssueId }, { "elapsedSeconds", mTotalElapsedTime } });
		if (mInterruptedAt.isValid()) {
			mInterruptedAt = QDateTime();
			emit interruptedAtChanged();
		}
		emit trackingChanged();
		emit pausedChanged();
		if (mStartedAt.isValid()) {
			mStartedAt = QDateTime();
			emit startedAtChanged();
		}
	}
}

void TimeTracker::pauseTracking()
{
	if (mTimer.isActive() && !mIsPaused) {
		mTotalElapsedTime += mStartTime.secsTo(QDateTime::currentDateTime());
		mTimer.stop();
		mIsPaused = true;
		saveTrackingState();
		QR_INFO(lcTimeTracker,
			QStringLiteral("Paused tracking issue %1 at %2 s").arg(mCurrentIssueId).arg(mTotalElapsedTime),
			{ { "issueId", mCurrentIssueId }, { "elapsedSeconds", mTotalElapsedTime } });
		emit pausedChanged();
		emit trackingChanged();
	}
}

void TimeTracker::resumeTracking()
{
	if (!mTimer.isActive() && mIsPaused) {
		mStartTime = QDateTime::currentDateTime();
		mLastTick = QDateTime();
		mTimer.start();
		mIsPaused = false;
		saveTrackingState();
		QR_INFO(lcTimeTracker,
			QStringLiteral("Resumed tracking issue %1 at %2 s").arg(mCurrentIssueId).arg(mTotalElapsedTime),
			{ { "issueId", mCurrentIssueId }, { "elapsedSeconds", mTotalElapsedTime } });
		emit pausedChanged();
		emit trackingChanged();
	}
}

// A date given as "yyyy-MM-dd..." text, or as a Date from QML (which arrives as QDateTime).
static QDate dateOf(const QVariant& value)
{
	if (value.metaType() == QMetaType::fromType<QDateTime>()) {
		return value.toDateTime().date();
	}
	if (value.metaType() == QMetaType::fromType<QDate>()) {
		return value.toDate();
	}
	return QDate::fromString(value.toString().left(10), Qt::ISODate);
}

int TimeTracker::addRecentEntry(const QVariantMap& entry)
{
	const auto hours = entry["hours"].toDouble();
	const auto comment = entry["comments"].toString();
	const auto issueId = entry["issueId"].toInt();
	const QDate day = dateOf(entry["date"]);
	const QDateTime spentOn(day.isValid() ? day : QDate::currentDate(), QTime(0, 0));

	// Redmine books no entry without an issue or without time, and a NaN or an infinity would trip
	// SQLite's NOT NULL on hours: refuse it here rather than saving what can only be rejected.
	if (issueId <= 0 || !std::isfinite(hours) || hours <= 0.0) {
		QR_WARNING(lcTimeTracker,
			QStringLiteral("Refused a new local time entry (issue %1, %2 h)").arg(issueId).arg(hours),
			{ { "issueId", issueId }, { "hours", hours } });
		return -1;
	}

	const int localId = Database::addTimeEntry(issueId, hours, comment, spentOn, entry["activityId"].toInt(),
		entry["customFields"].toList(), entry["issueStatusId"].toInt());
	if (localId < 0) {
		qWarning(lcTimeTracker) << "Could not save to database!";
		return -1;
	}

	QR_INFO(lcTimeTracker,
		QStringLiteral("Saved local time entry %1 (issue %2, %3 h)").arg(localId).arg(issueId).arg(hours),
		{ { "localId", localId }, { "issueId", issueId }, { "hours", hours }, { "date", day.toString(Qt::ISODate) } });
	QR_DEBUG(lcTimeTrackerContent, QStringLiteral("Comment of local time entry %1").arg(localId),
		{ { "localId", localId }, { "comment", comment } });

	// Reload rather than prepending the new entry ourselves, so it gets the same fields as the
	// others, including its issue's project (see Database::getRecentEntries()).
	loadRecentEntries();
	mInteractiveSubmissions.insert(localId);
	submitEntry(Database::getTimeEntry(localId));
	return localId;
}

//! Wait after the first failed submission of an entry; doubled with each further one up to
//! SUBMISSION_BACKOFF_MAX_SECONDS
static constexpr int SUBMISSION_BACKOFF_BASE_SECONDS = 2 * 60;
static constexpr int SUBMISSION_BACKOFF_MAX_SECONDS = 60 * 60;

bool TimeTracker::isDue(int localId) const
{
	const auto backoff = mBackoff.constFind(localId);
	return backoff == mBackoff.constEnd() || backoff->nextAttempt <= QDateTime::currentDateTime();
}

bool TimeTracker::hasDueEntries() const
{
	const QJsonArray pending = Database::getPendingEntries();
	return std::any_of(pending.cbegin(), pending.cend(),
		[this](const QJsonValue& entry) { return isDue(entry.toObject()["id"].toInt()); });
}

void TimeTracker::resetSubmissionBackoff()
{
	QR_DEBUG(lcTimeTracker, QStringLiteral("Reset the wait before submitting %1 entries again").arg(mBackoff.size()),
		{ { "entries", static_cast<int>(mBackoff.size()) } });
	mBackoff.clear();
}

void TimeTracker::connectionRestored()
{
	int reset = 0;
	for (auto it = mBackoff.begin(); it != mBackoff.end();) {
		if (it->notReached) {
			it = mBackoff.erase(it);
			++reset;
		} else {
			++it;
		}
	}
	QR_DEBUG(lcTimeTracker, QStringLiteral("Connection restored, reset the wait of %1 entries").arg(reset),
		{ { "reset", reset }, { "stillWaiting", static_cast<int>(mBackoff.size()) } });
}

void TimeTracker::submitPendingEntries()
{
	const QJsonArray pending = Database::getPendingEntries();
	int due = 0;
	for (const QJsonValue& entry : pending) {
		if (isDue(entry.toObject()["id"].toInt())) {
			++due;
			submitEntry(entry.toObject());
		}
	}
	if (!pending.isEmpty()) {
		QR_DEBUG(lcTimeTracker, QStringLiteral("Submitting pending entries: %1 due of %2").arg(due).arg(pending.size()),
			{ { "pending", pending.size() }, { "due", due }, { "waiting", static_cast<int>(pending.size()) - due } });
	}
}

void TimeTracker::submitEntry(const QJsonObject& entry)
{
	const int localId = entry["id"].toInt();
	if (localId <= 0 || mSubmitting.contains(localId)) {
		QR_DEBUG(lcTimeTracker, QStringLiteral("Entry %1 is being submitted already").arg(localId),
			{ { "localId", localId } });
		return;
	}
	mSubmitting.insert(localId);
	QR_INFO(lcTimeTracker,
		QStringLiteral("Submitting local time entry %1 (issue %2, %3 h)")
			.arg(localId)
			.arg(entry["issue_id"].toInt())
			.arg(entry["hours"].toDouble()),
		{ { "localId", localId }, { "issueId", entry["issue_id"].toInt() }, { "hours", entry["hours"].toDouble() },
			{ "date", entry["spent_on"].toString().left(10) },
			{ "previousFailures", mBackoff.value(localId).failures } });
	QR_DEBUG(lcTimeTrackerContent, QStringLiteral("Comment of local time entry %1").arg(localId),
		{ { "localId", localId }, { "comment", entry["comments"].toString() } });
	emit submitTimeEntry(localId, entry["issue_id"].toInt(), entry["hours"].toDouble(), entry["comments"].toString(),
		entry["spent_on"].toString().left(10), entry["activity_id"].toInt(),
		entry["custom_fields"].toArray().toVariantList());
}

void TimeTracker::submissionFailed(int localId, int httpStatus, const QString& error, int retryAfter)
{
	mSubmitting.remove(localId);
	mEditedWhileSubmitting.remove(localId); // sent again later with the current values anyway
	mInteractiveSubmissions.remove(localId);
	// Redmine refused the entry itself (e.g. 422 for a closed project, 403 without permission):
	// sending it unchanged again won't help. Anything else (not reached, 401 for a key being
	// fixed, server errors) is tried again later.
	if (httpStatus >= 400 && httpStatus < 500 && httpStatus != 401 && httpStatus != 408 && httpStatus != 429) {
		mBackoff.remove(localId);
		QR_WARNING(lcTimeTracker,
			QStringLiteral("Redmine rejected local time entry %1 (HTTP %2)").arg(localId).arg(httpStatus),
			{ { "localId", localId }, { "status", httpStatus }, { "error", error } });
		if (Database::setSyncStatus(localId, Database::Rejected, error)) {
			loadRecentEntries();
		}
		return;
	}
	// Wait 2 min, 4, 8, ... up to an hour before the next automatic try, or as long as the server asked
	SubmissionBackoff& backoff = mBackoff[localId];
	++backoff.failures;
	backoff.notReached = httpStatus == 0;
	const int exponent = std::min(backoff.failures - 1, 16); // keeps the shift in range
	const int wait =
		std::max(std::min(SUBMISSION_BACKOFF_BASE_SECONDS << exponent, SUBMISSION_BACKOFF_MAX_SECONDS), retryAfter);
	backoff.nextAttempt = QDateTime::currentDateTime().addSecs(wait);
	QR_LOG(lcTimeTracker, httpStatus == 0 ? QtInfoMsg : QtWarningMsg,
		QStringLiteral("Submitting local time entry %1 failed (HTTP %2), next try in %3 s")
			.arg(localId)
			.arg(httpStatus)
			.arg(wait),
		{ { "localId", localId }, { "status", httpStatus }, { "error", error }, { "failures", backoff.failures },
			{ "retryAfterSeconds", retryAfter }, { "waitSeconds", wait },
			{ "nextAttempt", backoff.nextAttempt.toString(Qt::ISODate) } });
}

bool TimeTracker::deleteRecentEntry(int localId)
{
	// Its creation on Redmine may be under way, and would then leave a Redmine entry nothing
	// points to.
	if (mSubmitting.contains(localId)) {
		return false;
	}
	mBackoff.remove(localId);
	if (Database::deleteTimeEntry(localId) < 0) {
		qCWarning(lcTimeTracker) << "Could not delete local time entry" << localId;
		return false;
	}
	loadRecentEntries();
	return true;
}

void TimeTracker::removeDeletedTimeEntry(int redmineId)
{
	if (Database::deleteTimeEntriesByRedmineId(redmineId) > 0) {
		loadRecentEntries();
	}
	const auto removed = std::remove_if(mEntriesFromId.begin(), mEntriesFromId.end(),
		[redmineId](const QVariant& entry) { return entry.toMap()["id"].toInt() == redmineId; });
	if (removed != mEntriesFromId.end()) {
		mEntriesFromId.erase(removed, mEntriesFromId.end());
		emit entriesFromIdChanged();
	}
}

void TimeTracker::sendToRedmine(int localId)
{
	QR_DEBUG(lcTimeTracker, QStringLiteral("Sending local time entry %1 to Redmine on request").arg(localId),
		{ { "localId", localId } });
	mBackoff.remove(localId);
	if (Database::setSyncStatus(localId, Database::Pending)) {
		loadRecentEntries();
		submitEntry(Database::getTimeEntry(localId));
	}
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
	const auto projectName = issue["project"]["name"].toString();
	const auto status = issue["status"]["id"].toInt();
	const auto description = issue["description"].toString();
	const auto createdOn = issue["created_on"].toVariant().toDateTime();
	const auto updatedOn = issue["updated_on"].toVariant().toDateTime();

	if (!Database::addIssue(id, subject, project, projectName, status, description, createdOn, updatedOn)) {
		QR_WARNING(lcTimeTracker, QStringLiteral("Could not add issue %1 to the cache").arg(id), { { "issueId", id } });
	} else {
		emit issueCacheChanged();
		// Recent entries show their issue's project, which may only now be known or have changed.
		// Reload only then: reloading rebuilds the list and loses its scroll position.
		const bool shownProjectChanged =
			std::any_of(mRecentEntries.cbegin(), mRecentEntries.cend(), [&](const QVariant& entry) {
				const QVariantMap map = entry.toMap();
				return map["issue_id"].toInt() == id && map["project_name"].toString() != projectName;
			});
		if (shownProjectChanged) {
			loadRecentEntries();
		}
	}
}

void TimeTracker::forgetIssue(int issueId)
{
	// Kept while it's tracked, so the tracking bar keeps offering its project's activities; it's
	// found unknown again once picked or refreshed later.
	if (isTracking() && mCurrentIssueId == issueId) {
		return;
	}
	const bool wasFavorite = isFavoriteIssue(issueId);
	if (wasFavorite) {
		Database::removeFavoriteIssue(issueId);
	}
	if (Database::removeIssue(issueId) > 0 || wasFavorite) {
		qCInfo(lcTimeTracker) << "Issue" << issueId << "is unknown to Redmine, removed it from the cache";
		emit issueCacheChanged(); // also reloads the favorites
		loadRecentEntries(); // its entries no longer show a project
	}
}

void TimeTracker::loadRecentEntries()
{
	const auto entries = Database::getRecentEntries();
	QVariantList recentEntries;
	for (const QJsonValue& value : entries) {
		recentEntries.append(value.toObject().toVariantMap());
	}
	// Also when it became empty, e.g. after deleting the last entry.
	if (recentEntries != mRecentEntries) {
		mRecentEntries = recentEntries;
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

QVariantList TimeTracker::issueIdsWithoutProject() const
{
	QVariantList ids;
	for (const int id : Database::issueIdsWithoutProject()) {
		ids.append(id);
	}
	return ids;
}

//! How far back unlinked entries are linked (see linkExistingTimeEntries()). Older ones are left alone,
//! so an entry that never matches doesn't make every launch fetch months of time entries.
static constexpr int LINKING_MAX_AGE_DAYS = 31;

QVariantList TimeTracker::linkCandidates() const
{
	// Recent unlinked entries of the last month, and every entry still to be sent (Redmine may
	// have it although the app didn't hear back), however old. Not limited further: whether an entry
	// reached Redmine isn't recorded (the reply may be lost to a timeout or a crash, and every entry is
	// sent at once when added), and sending one that is on Redmine already would duplicate it.
	const QString oldest = QDate::currentDate().addDays(-LINKING_MAX_AGE_DAYS).toString(Qt::ISODate);
	QVariantList candidates;
	QSet<int> ids;
	for (const QVariant& value : mRecentEntries) {
		const QVariantMap entry = value.toMap();
		if (entry["redmine_id"].toInt() <= 0 && entry["spent_on"].toString().left(10) >= oldest) {
			candidates.append(entry);
			ids.insert(entry["id"].toInt());
		}
	}
	for (const QJsonValue& value : Database::getPendingEntries()) {
		if (!ids.contains(value.toObject()["id"].toInt())) {
			candidates.append(value.toObject().toVariantMap());
		}
	}
	return candidates;
}

QVariantMap TimeTracker::unlinkedEntriesRange() const
{
	QString from;
	QString to;
	for (const QVariant& value : linkCandidates()) {
		const QString day = value.toMap()["spent_on"].toString().left(10);
		if (from.isEmpty() || day < from) {
			from = day;
		}
		if (to.isEmpty() || day > to) {
			to = day;
		}
	}
	if (from.isEmpty()) {
		return {};
	}
	return { { "from", from }, { "to", to } };
}

bool TimeTracker::hasPendingEntries() const { return !Database::getPendingEntries().isEmpty(); }

void TimeTracker::linkExistingTimeEntries(const QJsonArray& timeEntries)
{
	QSet<int> taken = Database::linkedRedmineIds();
	bool linked = false;
	int linkedCount = 0;
	const QVariantList candidates = linkCandidates();
	for (const QVariant& value : candidates) {
		const QVariantMap entry = value.toMap();
		if (mSubmitting.contains(entry["id"].toInt())) {
			continue; // being sent right now: linked by its own reply
		}
		const int issueId = entry["issue_id"].toInt();
		const QString day = entry["spent_on"].toString().left(10);
		const double hours = entry["hours"].toDouble();
		const QString comments = entry["comments"].toString().trimmed();

		// Only an entry alike in every compared field counts: one with another comment may be an
		// unrelated booking (e.g. made in the web UI after this entry's own submission failed), and
		// linking to it would let edits here overwrite it. Alike entries are interchangeable, so
		// the first one not linked yet will do.
		const auto match = std::find_if(timeEntries.begin(), timeEntries.end(), [&](const QJsonValue& remoteValue) {
			const QJsonObject remote = remoteValue.toObject();
			return !taken.contains(remote["id"].toInt()) && remote["issue"].toObject()["id"].toInt() == issueId &&
				remote["spent_on"].toString() == day && std::abs(remote["hours"].toDouble() - hours) < 0.01 &&
				remote["comments"].toString().trimmed() == comments;
		});
		if (match == timeEntries.end()) {
			continue;
		}
		const int redmineId = match->toObject()["id"].toInt();
		if (Database::updateSyncStatus(entry["id"].toInt(), redmineId)) {
			taken.insert(redmineId);
			linked = true;
			++linkedCount;
			applyIssueStatus(entry["id"].toInt());
		}
	}
	if (!candidates.isEmpty()) {
		QR_INFO(lcTimeTracker,
			QStringLiteral("Linked %1 of %2 local entries to time entries on Redmine")
				.arg(linkedCount)
				.arg(candidates.size()),
			{ { "linked", linkedCount }, { "candidates", static_cast<int>(candidates.size()) },
				{ "remoteEntries", timeEntries.size() } });
	}
	if (linked) {
		loadRecentEntries();
	}
}

void TimeTracker::updateRecentEntry(int localId, const QVariantMap& changes)
{
	const QJsonObject entry = Database::getTimeEntry(localId);
	// Redmine books no entry without time (and SQLite can't store a NaN): keep the entry as it is.
	const double hours = changes["hours"].toDouble();
	if (!std::isfinite(hours) || hours <= 0.0) {
		QR_WARNING(lcTimeTracker,
			QStringLiteral("Refused to change local time entry %1 to %2 h").arg(localId).arg(hours),
			{ { "localId", localId }, { "hours", hours } });
		return;
	}
	// Changed custom field values replace the entry's values of those fields; the others stay.
	QVariant customFields;
	if (changes.contains("custom_fields")) {
		QVariantList fields = entry["custom_fields"].toArray().toVariantList();
		for (const QVariant& changedField : changes["custom_fields"].toList()) {
			const int fieldId = changedField.toMap()["id"].toInt();
			fields.erase(std::remove_if(fields.begin(), fields.end(),
							 [fieldId](const QVariant& field) { return field.toMap()["id"].toInt() == fieldId; }),
				fields.end());
			fields.append(changedField);
		}
		customFields = fields;
	}

	const QDateTime spentOn(QDate::fromString(changes["spent_on"].toString(), Qt::ISODate), QTime(0, 0));
	const int changed = Database::updateTimeEntry(
		localId, hours, changes["comments"].toString(), spentOn, changes["activity_id"].toInt(), customFields);
	if (changed < 0) {
		qCWarning(lcTimeTracker) << "Could not update local time entry" << localId;
		return;
	}
	// Changed, so worth another try right away (when next sent)
	mBackoff.remove(localId);
	// A rejected entry is sent again once changed (hopefully fixed); an unconfirmed one only when
	// asked to, since Redmine may have it already.
	const int status = entry["sync_status"].toInt();
	// Being sent with the old values: send the changes once Redmine created it (linkCreatedTimeEntry()).
	if (status == Database::Pending && mSubmitting.contains(localId)) {
		mEditedWhileSubmitting.insert(localId);
	}
	if (status == Database::Rejected || (status == Database::Unconfirmed && changes["submit"].toBool())) {
		sendToRedmine(localId);
	} else if (changed > 0) {
		loadRecentEntries();
	}
}

int TimeTracker::stageRedmineUpdate(int redmineId, const QVariantMap& changes)
{
	// A confirmation that never comes (the server went away, the entry was deleted meanwhile) would be
	// kept forever: let the oldest go, they're the ones least likely to be confirmed by now.
	while (mPendingRedmineUpdates.size() >= MAX_PENDING_REDMINE_UPDATES) {
		const int dropped = mPendingRedmineUpdates.firstKey();
		QR_DEBUG(lcTimeTracker, QStringLiteral("Forgetting the confirmation of staged Redmine update %1").arg(dropped),
			{ { "tag", dropped }, { "redmineId", mPendingRedmineUpdates.value(dropped)["redmine_id"].toInt() } });
		mPendingRedmineUpdates.erase(mPendingRedmineUpdates.begin());
	}
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

	const int changed = Database::updateTimeEntriesByRedmineId(redmineId, hours, comments,
		QDateTime(QDate::fromString(spentOn, Qt::ISODate), QTime(0, 0)), changes["activity_id"].toInt());
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
		// Changed custom field values ([{id, value}]) replace the entry's values of those fields, or
		// are added if the entry doesn't have them yet.
		if (changes.contains("custom_fields")) {
			QVariantList fields = entry["custom_fields"].toList();
			for (const auto& changedField : changes["custom_fields"].toList()) {
				const QVariantMap change = changedField.toMap();
				const auto existing = std::find_if(fields.begin(), fields.end(),
					[&change](const QVariant& field) { return field.toMap()["id"].toInt() == change["id"].toInt(); });
				if (existing == fields.end()) {
					fields.append(change);
					continue;
				}
				QVariantMap field = existing->toMap();
				field["value"] = change["value"];
				*existing = field;
			}
			entry["custom_fields"] = fields;
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

void TimeTracker::applyIssueStatus(int localId)
{
	const QJsonObject entry = Database::getTimeEntry(localId);
	const int statusId = entry["issue_status_id"].toInt();
	if (statusId > 0 && Database::clearIssueStatus(localId)) {
		emit setIssueStatus(entry["issue_id"].toInt(), statusId);
	}
}

bool TimeTracker::linkCreatedTimeEntry(const QJsonObject& timeEntry, int localId)
{
	mSubmitting.remove(localId);
	mBackoff.remove(localId);
	const bool interactive = mInteractiveSubmissions.remove(localId);
	const int redmineId = timeEntry["id"].toInt(-1);
	if (redmineId < 0 || localId <= 0) {
		return interactive;
	}
	QR_INFO(lcTimeTracker, QStringLiteral("Local time entry %1 created on Redmine as %2").arg(localId).arg(redmineId),
		{ { "localId", localId }, { "redmineId", redmineId }, { "interactive", interactive },
			{ "editedWhileSubmitting", mEditedWhileSubmitting.contains(localId) } });
	if (Database::updateSyncStatus(localId, redmineId)) {
		loadRecentEntries();
	}
	applyIssueStatus(localId);
	// Changed while it was being sent: Redmine got the old values, so send the current ones.
	if (mEditedWhileSubmitting.remove(localId)) {
		const QJsonObject entry = Database::getTimeEntry(localId);
		const QVariantMap changes { { "hours", entry["hours"].toDouble() },
			{ "comments", entry["comments"].toString() }, { "spent_on", entry["spent_on"].toString().left(10) },
			{ "activity_id", entry["activity_id"].toInt() },
			{ "custom_fields", entry["custom_fields"].toArray().toVariantList() } };
		const int tag = stageRedmineUpdate(redmineId, changes);
		emit updateTimeEntryOnRedmine(redmineId, changes["hours"].toDouble(), changes["comments"].toString(),
			changes["spent_on"].toString(), changes["activity_id"].toInt(), changes["custom_fields"].toList(), tag);
	}
	return interactive;
}

void TimeTracker::updateElapsedTime() { noticeTick(QDateTime::currentDateTime()); }

void TimeTracker::noticeTick(const QDateTime& now)
{
	// The timer ticks every second; a much longer gap means the computer was asleep (suspended or
	// hibernated). Like time the app was gone, that may not be working time: ask (interruptedAt).
	if (mLastTick.isValid() && mLastTick.secsTo(now) > INTERRUPTION_THRESHOLD_SECONDS && !mInterruptedAt.isValid()) {
		mInterruptedAt = mLastTick;
		mResumedAt = now;
		mInterruptedBySleep = true;
		QR_INFO(lcTimeTracker,
			QStringLiteral("The computer seems to have slept for %1 s while tracking").arg(mLastTick.secsTo(now)),
			{ { "issueId", mCurrentIssueId }, { "gapSeconds", mLastTick.secsTo(now) },
				{ "lastTick", mLastTick.toString(Qt::ISODate) } });
		emit interruptedAtChanged();
	}
	mLastTick = now;

	// Lets a restart tell how long the app was gone (see restoreTrackingState()).
	if (++mSecondsSinceTouch >= 30) {
		mSecondsSinceTouch = 0;
		Database::touchTrackingState();
	}
	emit elapsedTimeChanged();
}
}
