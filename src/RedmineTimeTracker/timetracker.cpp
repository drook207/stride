// timetracker.cpp
#include "timetracker.h"
#include "database.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSet>
#include <algorithm>
#include <cmath>

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
	restoreTrackingState();
	loadFavoriteIssues();
	// A pinned issue's subject and project may only be known once the issue is cached.
	connect(this, &TimeTracker::issueAddedOrUpdated, this, &TimeTracker::loadFavoriteIssues);
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

void TimeTracker::saveTrackingState()
{
	if (!isTracking()) {
		Database::clearTrackingState();
		return;
	}
	Database::saveTrackingState({ { "issue_id", mCurrentIssueId },
		{ "started_at", mTimer.isActive() ? mStartTime : QDateTime() }, { "elapsed_seconds", mTotalElapsedTime },
		{ "comment", mTrackingComment }, { "activity_id", mTrackingActivityId } });
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
	if (!mIsPaused) {
		mTimer.start();
		const QDateTime lastSeen = state["last_seen"].toDateTime();
		if (lastSeen.isValid() && lastSeen.secsTo(QDateTime::currentDateTime()) > INTERRUPTION_THRESHOLD_SECONDS) {
			mInterruptedAt = lastSeen;
		}
	}
	qCInfo(lcTimeTracker) << "Restored the tracker for issue" << mCurrentIssueId
						  << (mIsPaused ? "(paused)" : "(running)");
}

void TimeTracker::resolveInterruption(bool countTimeAway)
{
	if (!mInterruptedAt.isValid()) {
		return;
	}
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
	mLastTick = QDateTime();
	mTimer.start();
	saveTrackingState();

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
		saveTrackingState();
		if (mInterruptedAt.isValid()) {
			mInterruptedAt = QDateTime();
			emit interruptedAtChanged();
		}
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
		saveTrackingState();
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

	const int localId = Database::addTimeEntry(issueId, hours, comment, spentOn, entry["activityId"].toInt(),
		entry["customFields"].toList(), entry["issueStatusId"].toInt());
	if (localId < 0) {
		qWarning(lcTimeTracker) << "Could not save to database!";
		return -1;
	}

	// Reload rather than prepending the new entry ourselves, so it gets the same fields as the
	// others, including its issue's project (see Database::getRecentEntries()).
	loadRecentEntries();
	mInteractiveSubmissions.insert(localId);
	submitEntry(Database::getTimeEntry(localId));
	return localId;
}

void TimeTracker::submitPendingEntries()
{
	for (const QJsonValue& entry : Database::getPendingEntries()) {
		submitEntry(entry.toObject());
	}
}

void TimeTracker::submitEntry(const QJsonObject& entry)
{
	const int localId = entry["id"].toInt();
	if (localId <= 0 || mSubmitting.contains(localId)) {
		return;
	}
	mSubmitting.insert(localId);
	emit submitTimeEntry(localId, entry["issue_id"].toInt(), entry["hours"].toDouble(), entry["comments"].toString(),
		entry["spent_on"].toString().left(10), entry["activity_id"].toInt(),
		entry["custom_fields"].toArray().toVariantList());
}

void TimeTracker::submissionFailed(int localId, int httpStatus, const QString& error)
{
	mSubmitting.remove(localId);
	mEditedWhileSubmitting.remove(localId); // sent again later with the current values anyway
	mInteractiveSubmissions.remove(localId);
	// Redmine refused the entry itself (e.g. 422 for a closed project, 403 without permission):
	// sending it unchanged again won't help. Anything else (not reached, 401 for a key being
	// fixed, server errors) is tried again later.
	if (httpStatus >= 400 && httpStatus < 500 && httpStatus != 401 && httpStatus != 408 && httpStatus != 429) {
		if (Database::setSyncStatus(localId, Database::Rejected, error)) {
			loadRecentEntries();
		}
	}
}

bool TimeTracker::deleteRecentEntry(int localId)
{
	// Its creation on Redmine may be under way, and would then leave a Redmine entry nothing
	// points to.
	if (mSubmitting.contains(localId)) {
		return false;
	}
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
		qWarning() << "Could not add issue  " << issue;
	} else {
		emit issueAddedOrUpdated();
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
	if (Database::removeIssue(issueId) > 0) {
		qCInfo(lcTimeTracker) << "Issue" << issueId << "is unknown to Redmine, removed it from the cache";
		emit issueAddedOrUpdated();
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
	// have it although the app didn't hear back), however old.
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
	for (const QVariant& value : linkCandidates()) {
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
			applyIssueStatus(entry["id"].toInt());
		}
	}
	if (linked) {
		loadRecentEntries();
	}
}

void TimeTracker::updateRecentEntry(int localId, const QVariantMap& changes)
{
	const QJsonObject entry = Database::getTimeEntry(localId);
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
	const int changed = Database::updateTimeEntry(localId, changes["hours"].toDouble(), changes["comments"].toString(),
		spentOn, changes["activity_id"].toInt(), customFields);
	if (changed < 0) {
		qCWarning(lcTimeTracker) << "Could not update local time entry" << localId;
		return;
	}
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
	const bool interactive = mInteractiveSubmissions.remove(localId);
	const int redmineId = timeEntry["id"].toInt(-1);
	if (redmineId < 0 || localId <= 0) {
		return interactive;
	}
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
