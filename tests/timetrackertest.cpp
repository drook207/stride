#include "timetracker.h"
#include "database.h"
#include <QJsonDocument>
#include <QSqlQuery>
#include <cmath>
#include <gtest/gtest.h>

using Stride::Database;
using Stride::TimeTracker;

//! Counts how often a signal was emitted (QtTest's SignalCounter isn't linked).
class SignalCounter {
public:
	template <typename Sender, typename Signal> SignalCounter(Sender* sender, Signal signal)
	{
		QObject::connect(sender, signal, sender, [this] { ++mCount; });
	}

	int count() const { return mCount; }

private:
	int mCount = 0;
};

class TimeTrackerTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		// TimeTracker opens the database itself; it finds this one already open.
		ASSERT_TRUE(Database::initialize(QStringLiteral(":memory:")));
		tracker = std::make_unique<TimeTracker>();
	}

	void TearDown() override
	{
		tracker.reset();
		Database::close();
	}

	//! Adds a local entry dated daysAgo days back; returns its local id. It's being sent to Redmine
	//! then (see TimeTracker::addRecentEntry()).
	int addEntry(int issueId, double hours, const QString& comments, int daysAgo = 1)
	{
		return tracker->addRecentEntry(
			{ { "issueId", issueId }, { "hours", hours }, { "comments", comments }, { "date", day(daysAgo) } });
	}

	//! Adds a local entry whose sending failed without reaching Redmine (e.g. offline).
	int addOfflineEntry(int issueId, double hours, const QString& comments, int daysAgo = 1)
	{
		const int id = addEntry(issueId, hours, comments, daysAgo);
		tracker->submissionFailed(id, 0, "offline");
		return id;
	}

	static QString day(int daysAgo) { return QDate::currentDate().addDays(-daysAgo).toString(Qt::ISODate); }

	//! A time entry as Redmine lists it.
	static QJsonObject remote(int id, int issueId, double hours, const QString& comments, int daysAgo = 1)
	{
		return { { "id", id }, { "issue", QJsonObject { { "id", issueId } } }, { "hours", hours },
			{ "comments", comments }, { "spent_on", day(daysAgo) } };
	}

	//! redmine_id of the recent entry with the given local id (0 if unlinked).
	int redmineIdOf(int localId) const
	{
		for (const QVariant& entry : tracker->recentEntries()) {
			if (entry.toMap()["id"].toInt() == localId) {
				return entry.toMap()["redmine_id"].toInt();
			}
		}
		return -1;
	}

	std::unique_ptr<TimeTracker> tracker;
};

TEST_F(TimeTrackerTest, RefusesANewEntryWithoutAnIssueOrWithoutTime)
{
	// Redmine books neither, and a NaN can't be stored in the hours column.
	EXPECT_EQ(addEntry(0, 1.0, "no issue"), -1);
	EXPECT_EQ(addEntry(-3, 1.0, "negative issue"), -1);
	EXPECT_EQ(addEntry(10, 0.0, "no time"), -1);
	EXPECT_EQ(addEntry(10, -0.5, "negative time"), -1);
	EXPECT_EQ(addEntry(10, std::nan(""), "not a number"), -1);

	EXPECT_TRUE(tracker->recentEntries().isEmpty());
	EXPECT_FALSE(tracker->hasPendingEntries());

	// A valid one still goes through.
	EXPECT_GE(addEntry(10, 0.5, "fine"), 0);
	EXPECT_EQ(tracker->recentEntries().size(), 1);
}

TEST_F(TimeTrackerTest, RefusesToChangeAnEntryToInvalidHours)
{
	const int local = addEntry(10, 1.5, "Mine");
	tracker->updateRecentEntry(local, { { "hours", 0.0 }, { "comments", "changed" }, { "spent_on", day(1) } });
	tracker->updateRecentEntry(local, { { "hours", std::nan("") }, { "comments", "changed" }, { "spent_on", day(1) } });

	const QJsonObject entry = Database::getTimeEntry(local);
	EXPECT_DOUBLE_EQ(entry["hours"].toDouble(), 1.5);
	EXPECT_EQ(entry["comments"].toString().toStdString(), "Mine");
}

TEST_F(TimeTrackerTest, ForgetsStagedRedmineUpdatesOlderThanTheNewestHundred)
{
	const int local = addEntry(10, 1.0, "Mine");
	tracker->linkCreatedTimeEntry(remote(500, 10, 1.0, "Mine"), local);

	const QVariantMap oldestChanges { { "hours", 2.0 }, { "comments", "oldest" }, { "spent_on", day(1) } };
	const int oldest = tracker->stageRedmineUpdate(500, oldestChanges);
	const QVariantMap laterChanges { { "hours", 2.0 }, { "comments", "later" }, { "spent_on", day(1) } };
	int newest = oldest;
	// More than the cap (100): the oldest ones are dropped, their confirmation can't be coming anymore.
	for (int i = 0; i < 100; ++i) {
		newest = tracker->stageRedmineUpdate(500, laterChanges);
	}

	tracker->applyRedmineUpdate(500, oldest);
	EXPECT_EQ(Database::getTimeEntry(local)["comments"].toString().toStdString(), "Mine");

	tracker->applyRedmineUpdate(500, newest);
	EXPECT_EQ(Database::getTimeEntry(local)["comments"].toString().toStdString(), "later");
}

TEST_F(TimeTrackerTest, LinksOnlyEntriesWithTheSameIssueDayHoursAndComment)
{
	const int exact = addOfflineEntry(10, 1.5, "Review");
	const int rounded = addOfflineEntry(11, 1.249, "Docs");
	const int otherComment = addOfflineEntry(12, 2.0, "Mine");

	tracker->linkExistingTimeEntries(QJsonArray {
		remote(100, 10, 1.5, "Review"), remote(101, 11, 1.25, "Docs"), remote(102, 12, 2.0, "Booked in the web UI") });

	EXPECT_EQ(redmineIdOf(exact), 100);
	EXPECT_EQ(redmineIdOf(rounded), 101);
	EXPECT_EQ(redmineIdOf(otherComment), 0);
}

TEST_F(TimeTrackerTest, LinksEachRedmineEntryOnlyOnce)
{
	const int older = addOfflineEntry(10, 1.0, "Same");
	const int newer = addOfflineEntry(10, 1.0, "Same");
	const int alreadyLinked = addEntry(20, 3.0, "Linked");
	tracker->linkCreatedTimeEntry(remote(300, 20, 3.0, "Linked"), alreadyLinked);

	tracker->linkExistingTimeEntries(QJsonArray { remote(200, 10, 1.0, "Same"), remote(300, 20, 3.0, "Linked") });

	// Exactly one of the identical entries gets the single Redmine entry.
	EXPECT_EQ(redmineIdOf(older) + redmineIdOf(newer), 200);
	EXPECT_EQ(redmineIdOf(alreadyLinked), 300);
}

TEST_F(TimeTrackerTest, UnlinkedRangeCoversTheLastMonthAndEverythingStillToSend)
{
	addEntry(1, 1.0, "recent", 3);
	addEntry(1, 1.0, "less recent", 10);
	// Old and unconfirmed: left alone. Old and still to be sent: included, however old.
	Database::setSyncStatus(addEntry(1, 1.0, "too old", 60), Database::Unconfirmed);

	QVariantMap range = tracker->unlinkedEntriesRange();
	EXPECT_EQ(range["from"].toString(), day(10));
	EXPECT_EQ(range["to"].toString(), day(3));

	addOfflineEntry(1, 1.0, "old, not sent yet", 90);
	range = tracker->unlinkedEntriesRange();
	EXPECT_EQ(range["from"].toString(), day(90));
}

TEST_F(TimeTrackerTest, StagedUpdatesApplyOnlyTheirOwnChangesAndRejectedOnesAreDropped)
{
	const int local = addEntry(10, 1.0, "Original");
	tracker->linkCreatedTimeEntry(remote(500, 10, 1.0, "Original"), local);

	const int first =
		tracker->stageRedmineUpdate(500, { { "hours", 2.0 }, { "comments", "First" }, { "spent_on", day(1) } });
	const int second =
		tracker->stageRedmineUpdate(500, { { "hours", 3.0 }, { "comments", "Second" }, { "spent_on", day(1) } });
	tracker->applyRedmineUpdate(500, first);
	tracker->discardRedmineUpdate(500, second);
	// A confirmation for the discarded update changes nothing any more.
	tracker->applyRedmineUpdate(500, second);

	const QVariantMap entry = tracker->recentEntries().first().toMap();
	EXPECT_EQ(entry["comments"].toString().toStdString(), "First");
	EXPECT_DOUBLE_EQ(entry["hours"].toDouble(), 2.0);
}

TEST_F(TimeTrackerTest, NewEntriesAreSentOnceAndStayPendingUntilRedmineIsReached)
{
	std::vector<int> sent;
	QObject::connect(tracker.get(), &TimeTracker::submitTimeEntry,
		[&](int localId, int, double, const QString&, const QString& spentOn, int activityId, const QVariantList&) {
			sent.push_back(localId);
			EXPECT_EQ(spentOn, day(1));
			EXPECT_EQ(activityId, 9);
		});

	const int local = tracker->addRecentEntry(
		{ { "issueId", 10 }, { "hours", 1.0 }, { "comments", "Offline" }, { "date", day(1) }, { "activityId", 9 } });
	ASSERT_EQ(sent, std::vector<int> { local });

	// Still being sent: not sent a second time.
	tracker->submitPendingEntries();
	EXPECT_EQ(sent.size(), 1u);

	// Not reached: stays pending, and is sent again later - once its wait is over (see
	// RetryableFailuresWaitBeforeTheNextAutomaticTry), here ended by hand.
	tracker->submissionFailed(local, 0, "offline");
	EXPECT_EQ(tracker->recentEntries().first().toMap()["sync_status"].toInt(), Database::Pending);
	tracker->submitPendingEntries();
	EXPECT_EQ(sent.size(), 1u);
	tracker->resetSubmissionBackoff();
	tracker->submitPendingEntries();
	EXPECT_EQ(sent.size(), 2u);

	EXPECT_FALSE(tracker->linkCreatedTimeEntry(remote(900, 10, 1.0, "Offline"), local)); // background
	EXPECT_EQ(redmineIdOf(local), 900);
	tracker->submitPendingEntries();
	EXPECT_EQ(sent.size(), 2u);
}

TEST_F(TimeTrackerTest, RetryableFailuresWaitBeforeTheNextAutomaticTry)
{
	std::vector<int> sent;
	QObject::connect(tracker.get(), &TimeTracker::submitTimeEntry,
		[&](int localId, int, double, const QString&, const QString&, int, const QVariantList&) {
			sent.push_back(localId);
		});

	// Not reached, key being fixed, timeout, rate limit, server errors
	for (const int status : { 0, 401, 408, 429, 500, 503 }) {
		const int local = addEntry(10, 1.0, QString("Status %0").arg(status));
		sent.clear();
		tracker->submissionFailed(local, status, "failed", 0);

		EXPECT_EQ(redmineIdOf(local), 0);
		EXPECT_FALSE(tracker->hasDueEntries()) << status;
		tracker->submitPendingEntries();
		EXPECT_TRUE(sent.empty()) << status;

		tracker->resetSubmissionBackoff();
		EXPECT_TRUE(tracker->hasDueEntries()) << status;
		tracker->submitPendingEntries();
		EXPECT_EQ(sent, std::vector<int> { local }) << status;
		tracker->submissionFailed(local, 422, "done with this one"); // out of the way
	}
}

TEST_F(TimeTrackerTest, RetryAfterAndRepeatedFailuresKeepAnEntryWaiting)
{
	int sent = 0;
	QObject::connect(tracker.get(), &TimeTracker::submitTimeEntry, [&]() { ++sent; });

	const int local = addEntry(10, 1.0, "Busy server");
	sent = 0;
	tracker->submissionFailed(local, 429, "Too many requests", 3600);
	tracker->submissionFailed(local, 503, "Unavailable", 0);
	tracker->submissionFailed(local, 0, "offline", 86400);

	EXPECT_FALSE(tracker->hasDueEntries());
	tracker->submitPendingEntries();
	EXPECT_EQ(sent, 0);
}

TEST_F(TimeTrackerTest, ConnectionRestoredEndsTheWaitOfEntriesThatDidNotReachTheServerOnly)
{
	std::vector<int> sent;
	QObject::connect(tracker.get(), &TimeTracker::submitTimeEntry,
		[&](int localId, int, double, const QString&, const QString&, int, const QVariantList&) {
			sent.push_back(localId);
		});

	const int offline = addEntry(10, 1.0, "Offline");
	const int broken = addEntry(10, 1.0, "Server error");
	tracker->submissionFailed(offline, 0, "offline");
	tracker->submissionFailed(broken, 503, "Unavailable");
	sent.clear();

	tracker->connectionRestored();
	tracker->submitPendingEntries();
	EXPECT_EQ(sent, std::vector<int> { offline });

	// A later server error takes the entry out of it again
	sent.clear();
	tracker->submissionFailed(offline, 0, "offline");
	tracker->submissionFailed(offline, 500, "Server error");
	tracker->connectionRestored();
	tracker->submitPendingEntries();
	EXPECT_TRUE(sent.empty());
}

TEST_F(TimeTrackerTest, HasDueEntriesIsFalseWithoutPendingEntries)
{
	EXPECT_FALSE(tracker->hasDueEntries());

	const int local = addEntry(10, 1.0, "Pending");
	EXPECT_TRUE(tracker->hasDueEntries()); // not waiting yet
	tracker->submissionFailed(local, 422, "Rejected");
	EXPECT_FALSE(tracker->hasDueEntries());
}

TEST_F(TimeTrackerTest, ChangingAnEntryEndsOnlyItsOwnWait)
{
	std::vector<int> sent;
	QObject::connect(tracker.get(), &TimeTracker::submitTimeEntry,
		[&](int localId, int, double, const QString&, const QString&, int, const QVariantList&) {
			sent.push_back(localId);
		});

	const int waiting = addOfflineEntry(10, 1.0, "Still waiting");
	const int changed = addOfflineEntry(11, 1.0, "Changed");
	sent.clear();
	ASSERT_FALSE(tracker->hasDueEntries());

	tracker->updateRecentEntry(changed, { { "hours", 2.0 }, { "comments", "Changed" }, { "spent_on", day(1) } });
	EXPECT_TRUE(tracker->hasDueEntries());
	tracker->submitPendingEntries();

	EXPECT_EQ(sent, std::vector<int> { changed });
	EXPECT_NE(waiting, changed);
}

TEST_F(TimeTrackerTest, SendingByHandIgnoresTheWait)
{
	std::vector<int> sent;
	QObject::connect(tracker.get(), &TimeTracker::submitTimeEntry,
		[&](int localId, int, double, const QString&, const QString&, int, const QVariantList&) {
			sent.push_back(localId);
		});

	const int local = addOfflineEntry(10, 1.0, "Offline");
	sent.clear();

	tracker->sendToRedmine(local);

	EXPECT_EQ(sent, std::vector<int> { local });
	// And the wait is over for the automatic sending as well, once that try fails again it starts anew
	tracker->submissionFailed(local, 0, "offline");
	EXPECT_FALSE(tracker->hasDueEntries());
}

TEST_F(TimeTrackerTest, WaitsAreNotKeptAcrossARestart)
{
	const int local = addOfflineEntry(10, 1.0, "Offline");
	ASSERT_FALSE(tracker->hasDueEntries());

	TimeTracker restarted;
	std::vector<int> sent;
	QObject::connect(&restarted, &TimeTracker::submitTimeEntry,
		[&](int localId, int, double, const QString&, const QString&, int, const QVariantList&) {
			sent.push_back(localId);
		});

	EXPECT_TRUE(restarted.hasDueEntries());
	restarted.submitPendingEntries();
	EXPECT_EQ(sent, std::vector<int> { local });
}

TEST_F(TimeTrackerTest, RejectedEntriesWaitUntilChanged)
{
	int sent = 0;
	QObject::connect(tracker.get(), &TimeTracker::submitTimeEntry, [&]() { ++sent; });

	const int local = addEntry(10, 1.0, "Closed project");
	tracker->submissionFailed(local, 422, "Project is closed");
	const QVariantMap entry = tracker->recentEntries().first().toMap();
	EXPECT_EQ(entry["sync_status"].toInt(), Database::Rejected);
	EXPECT_EQ(entry["sync_error"].toString().toStdString(), "Project is closed");

	tracker->submitPendingEntries();
	EXPECT_EQ(sent, 1);

	tracker->updateRecentEntry(local, { { "hours", 1.0 }, { "comments", "Other issue" }, { "spent_on", day(1) } });
	EXPECT_EQ(sent, 2);
	EXPECT_EQ(tracker->recentEntries().first().toMap()["sync_status"].toInt(), Database::Pending);
}

TEST_F(TimeTrackerTest, RunningTrackerIsRestoredWithItsDetails)
{
	tracker->startTracking(42);
	tracker->setTrackingComment("Reviewing");
	tracker->setTrackingActivityId(9);

	TimeTracker restored;
	EXPECT_TRUE(restored.isTracking());
	EXPECT_FALSE(restored.isPaused());
	EXPECT_EQ(restored.currentIssueId(), 42);
	EXPECT_EQ(restored.trackingComment().toStdString(), "Reviewing");
	EXPECT_EQ(restored.trackingActivityId(), 9);
	EXPECT_FALSE(restored.interruptedAt().isValid()); // the app was gone only briefly
}

TEST_F(TimeTrackerTest, PausedTrackerKeepsItsTimeAndStoppedOneIsForgotten)
{
	ASSERT_TRUE(
		Database::saveTrackingState({ { "issue_id", 7 }, { "started_at", QDateTime() }, { "elapsed_seconds", 5400 } }));
	{
		TimeTracker restored;
		EXPECT_TRUE(restored.isPaused());
		EXPECT_EQ(restored.elapsedTime().toStdString(), "01:30:00");
		restored.stopTracking();
	}
	TimeTracker afterStop;
	EXPECT_FALSE(afterStop.isTracking());
}

TEST_F(TimeTrackerTest, TimeWhileTheAppWasGoneCanBeLeftOut)
{
	const QDateTime now = QDateTime::currentDateTime();
	ASSERT_TRUE(Database::saveTrackingState(
		{ { "issue_id", 7 }, { "started_at", now.addSecs(-3 * 3600) }, { "elapsed_seconds", 600 } }));
	// The app was last seen two hours ago, one hour into the running stretch.
	QSqlQuery query;
	query.prepare("UPDATE tracking_state SET last_seen = ?");
	query.addBindValue(now.addSecs(-2 * 3600));
	ASSERT_TRUE(query.exec());

	TimeTracker restored;
	ASSERT_TRUE(restored.interruptedAt().isValid());
	restored.resolveInterruption(false);

	EXPECT_FALSE(restored.interruptedAt().isValid());
	EXPECT_TRUE(restored.isPaused());
	EXPECT_EQ(restored.elapsedTime().toStdString(), "01:10:00");
}

TEST_F(TimeTrackerTest, DeletesLocalEntriesAndForgetsOnesDeletedOnRedmine)
{
	const int local = addEntry(10, 1.0, "Local only");
	tracker->submissionFailed(local, 0, "offline"); // no longer being sent
	const int linked = addEntry(11, 2.0, "On Redmine");
	tracker->linkCreatedTimeEntry(remote(700, 11, 2.0, "On Redmine"), linked);
	tracker->addEntriesFromId(QJsonArray { remote(700, 11, 2.0, "On Redmine"), remote(701, 11, 1.0, "Other") });

	EXPECT_TRUE(tracker->deleteRecentEntry(local));
	tracker->removeDeletedTimeEntry(700);

	EXPECT_TRUE(tracker->recentEntries().isEmpty());
	ASSERT_EQ(tracker->entriesFromId().size(), 1);
	EXPECT_EQ(tracker->entriesFromId().first().toMap()["id"].toInt(), 701);
}

TEST_F(TimeTrackerTest, EntryBeingSentIsNotDeleted)
{
	const int local = addEntry(10, 1.0, "Sending");
	EXPECT_FALSE(tracker->deleteRecentEntry(local));
	EXPECT_EQ(tracker->recentEntries().size(), 1);
}

TEST_F(TimeTrackerTest, ContinueTrackingStartsOnlyWhenIdle)
{
	EXPECT_TRUE(tracker->continueTracking(42, "Same as yesterday", 9));
	EXPECT_EQ(tracker->currentIssueId(), 42);
	EXPECT_EQ(tracker->trackingComment().toStdString(), "Same as yesterday");
	EXPECT_EQ(tracker->trackingActivityId(), 9);

	// Already tracking: the running tracker stays.
	EXPECT_FALSE(tracker->continueTracking(43, "Other"));
	EXPECT_EQ(tracker->currentIssueId(), 42);
}

TEST_F(TimeTrackerTest, FavoriteIssuesAreToggledAndKept)
{
	ASSERT_TRUE(Database::addIssue(
		42, "Login page", 1, "Website", 1, "", QDateTime::currentDateTime(), QDateTime::currentDateTime()));
	tracker->toggleFavoriteIssue(42);
	tracker->toggleFavoriteIssue(7); // not cached
	ASSERT_EQ(tracker->favoriteIssues().size(), 2);
	EXPECT_TRUE(tracker->isFavoriteIssue(42));
	const QVariantList favorites = tracker->favoriteIssues();
	const auto pinned = std::find_if(favorites.cbegin(), favorites.cend(),
		[](const QVariant& favorite) { return favorite.toMap()["issue_id"].toInt() == 42; });
	ASSERT_NE(pinned, favorites.cend());
	EXPECT_EQ(pinned->toMap()["subject"].toString().toStdString(), "Login page");

	tracker->toggleFavoriteIssue(42);
	EXPECT_FALSE(tracker->isFavoriteIssue(42));

	TimeTracker restarted;
	ASSERT_EQ(restarted.favoriteIssues().size(), 1);
	EXPECT_EQ(restarted.favoriteIssues().first().toMap()["issue_id"].toInt(), 7);
}

TEST_F(TimeTrackerTest, SleepCanBeLeftOutWhileTrackingGoesOn)
{
	tracker->startTracking(42);
	const QDateTime now = QDateTime::currentDateTime();
	tracker->noticeTick(now.addSecs(-3600)); // last tick before the computer slept for an hour
	tracker->noticeTick(now);

	ASSERT_TRUE(tracker->interruptedAt().isValid());
	EXPECT_TRUE(tracker->interruptedBySleep());
	tracker->resolveInterruption(false);

	EXPECT_FALSE(tracker->interruptedAt().isValid());
	EXPECT_TRUE(tracker->isTracking());
	EXPECT_FALSE(tracker->isPaused());
	EXPECT_LT(tracker->getElapsedHours(), 0.01); // the hour asleep isn't counted
}

TEST_F(TimeTrackerTest, PendingEntryThatReachedRedmineIsLinkedInsteadOfSentAgain)
{
	int sent = 0;
	QObject::connect(tracker.get(), &TimeTracker::submitTimeEntry, [&]() { ++sent; });

	const int local = addEntry(10, 1.0, "Timed out", 60); // older than the month recent entries cover
	tracker->submissionFailed(local, 0, "timeout"); // Redmine saved it, the reply got lost

	const QVariantMap range = tracker->unlinkedEntriesRange();
	EXPECT_EQ(range["from"].toString(), day(60));
	EXPECT_TRUE(tracker->hasPendingEntries());

	tracker->linkExistingTimeEntries(QJsonArray { remote(800, 10, 1.0, "Timed out", 60) });
	tracker->submitPendingEntries();

	EXPECT_EQ(sent, 1); // only the first attempt
	EXPECT_EQ(redmineIdOf(local), 800);
	EXPECT_FALSE(tracker->hasPendingEntries());
}

TEST_F(TimeTrackerTest, ChangesMadeWhileSendingAreSentAfterwards)
{
	int updatedId = -1;
	QString updatedComment;
	QObject::connect(
		tracker.get(), &TimeTracker::updateTimeEntryOnRedmine, [&](int redmineId, double, const QString& comments) {
			updatedId = redmineId;
			updatedComment = comments;
		});

	const int local = addEntry(10, 1.0, "Old"); // being sent
	tracker->updateRecentEntry(local, { { "hours", 1.0 }, { "comments", "New" }, { "spent_on", day(1) } });
	tracker->linkCreatedTimeEntry(remote(900, 10, 1.0, "Old"), local);

	EXPECT_EQ(updatedId, 900);
	EXPECT_EQ(updatedComment.toStdString(), "New");
}

TEST_F(TimeTrackerTest, TimeWorkedBetweenWakingUpAndAnsweringCounts)
{
	tracker->startTracking(42);
	const QDateTime now = QDateTime::currentDateTime();
	tracker->noticeTick(now.addSecs(-3 * 3600)); // last tick before sleeping
	tracker->noticeTick(now.addSecs(-1800)); // woke up half an hour ago, worked since
	ASSERT_TRUE(tracker->interruptedBySleep());

	tracker->resolveInterruption(false); // answered just now

	EXPECT_NEAR(tracker->getElapsedHours(), 0.5, 0.01);
}

TEST_F(TimeTrackerTest, IssueStatusIsSetOnlyOnceTheEntryIsOnRedmine)
{
	QList<QPair<int, int>> statuses;
	QObject::connect(tracker.get(), &TimeTracker::setIssueStatus, [&](int issueId, int statusId) {
		statuses.append({ issueId, statusId });
	});

	const int rejected = tracker->addRecentEntry(
		{ { "issueId", 10 }, { "hours", 1.0 }, { "comments", "" }, { "date", day(1) }, { "issueStatusId", 3 } });
	tracker->submissionFailed(rejected, 422, "Closed project");
	EXPECT_TRUE(statuses.isEmpty());

	const int created = tracker->addRecentEntry(
		{ { "issueId", 11 }, { "hours", 1.0 }, { "comments", "" }, { "date", day(1) }, { "issueStatusId", 5 } });
	tracker->linkCreatedTimeEntry(remote(950, 11, 1.0, ""), created);
	tracker->linkCreatedTimeEntry(remote(950, 11, 1.0, ""), created); // set once only

	ASSERT_EQ(statuses.size(), 1);
	EXPECT_EQ(statuses.first(), qMakePair(11, 5));
}

TEST_F(TimeTrackerTest, IssuesRedmineDoesntKnowAreForgotten)
{
	ASSERT_TRUE(Database::addIssue(
		15, "Gone after a reset", 1, "Old project", 1, "", QDateTime::currentDateTime(), QDateTime::currentDateTime()));
	addOfflineEntry(15, 1.0, "Booked on it");
	ASSERT_EQ(tracker->recentEntries().first().toMap()["project_name"].toString().toStdString(), "Old project");

	int cacheChanges = 0;
	QObject::connect(tracker.get(), &TimeTracker::issueCacheChanged, [&]() { ++cacheChanges; });
	tracker->forgetIssue(15);
	tracker->forgetIssue(15); // not cached any more: nothing to do

	EXPECT_EQ(cacheChanges, 1);
	EXPECT_TRUE(tracker->recentEntries().first().toMap()["project_name"].toString().isEmpty());
	EXPECT_EQ(tracker->issueIdsWithoutProject(), QVariantList { 15 });
}

TEST_F(TimeTrackerTest, ForgottenIssueIsUnpinnedButNotWhileTracked)
{
	for (int id : { 15, 16 }) {
		ASSERT_TRUE(Database::addIssue(
			id, "Gone", 1, "Old", 1, "", QDateTime::currentDateTime(), QDateTime::currentDateTime()));
	}
	tracker->toggleFavoriteIssue(15);
	tracker->forgetIssue(15);
	EXPECT_FALSE(tracker->isFavoriteIssue(15));

	tracker->startTracking(16);
	tracker->forgetIssue(16); // still needed for the tracking bar's activities
	QSqlQuery query("SELECT COUNT(*) FROM issues WHERE id = 16");
	ASSERT_TRUE(query.next());
	EXPECT_EQ(query.value(0).toInt(), 1);
}

TEST_F(TimeTrackerTest, StartingSetsTheStartTime)
{
	EXPECT_FALSE(tracker->startedAt().isValid());
	SignalCounter spy(tracker.get(), &TimeTracker::startedAtChanged);
	tracker->startTracking(5);
	EXPECT_EQ(spy.count(), 1);
	EXPECT_LE(qAbs(tracker->startedAt().secsTo(QDateTime::currentDateTime())), 1);
}

TEST_F(TimeTrackerTest, ChangingTheStartShiftsTheTimeWhileRunningAndPaused)
{
	tracker->startTracking(5);
	SignalCounter startSpy(tracker.get(), &TimeTracker::startedAtChanged);
	SignalCounter elapsedSpy(tracker.get(), &TimeTracker::elapsedTimeChanged);
	const QDateTime now = QDateTime::currentDateTime();

	EXPECT_TRUE(tracker->setStartedAt(now.addSecs(-3600)));
	EXPECT_EQ(tracker->startedAt(), now.addSecs(-3600));
	EXPECT_EQ(startSpy.count(), 1);
	EXPECT_EQ(elapsedSpy.count(), 1);
	EXPECT_NEAR(tracker->getElapsedHours(), 1.0, 0.01);

	EXPECT_TRUE(tracker->setStartedAt(now.addSecs(-1800)));
	EXPECT_NEAR(tracker->getElapsedHours(), 0.5, 0.01);

	tracker->pauseTracking();
	EXPECT_TRUE(tracker->setStartedAt(now.addSecs(-5400)));
	EXPECT_TRUE(tracker->isPaused());
	EXPECT_NEAR(tracker->getElapsedHours(), 1.5, 0.01);

	TimeTracker restored;
	EXPECT_EQ(restored.startedAt(), now.addSecs(-5400));
	EXPECT_TRUE(restored.isPaused());
	EXPECT_NEAR(restored.getElapsedHours(), 1.5, 0.01);
}

TEST_F(TimeTrackerTest, UnacceptableStartsAreRejected)
{
	const QDateTime now = QDateTime::currentDateTime();
	EXPECT_FALSE(tracker->setStartedAt(now.addSecs(-60))); // not tracking
	EXPECT_FALSE(tracker->startedAt().isValid());

	tracker->startTracking(5);
	const QDateTime started = tracker->startedAt();
	SignalCounter spy(tracker.get(), &TimeTracker::startedAtChanged);
	EXPECT_FALSE(tracker->setStartedAt(QDateTime()));
	EXPECT_FALSE(tracker->setStartedAt(now.addSecs(3600)));
	EXPECT_FALSE(tracker->setStartedAt(now.addSecs(-25 * 3600)));
	EXPECT_FALSE(tracker->setStartedAt(started.addSecs(60))); // would leave negative time
	EXPECT_EQ(tracker->startedAt(), started);
	EXPECT_EQ(spy.count(), 0);
	EXPECT_EQ(tracker->elapsedTime().toStdString(), "00:00:00");
}

TEST_F(TimeTrackerTest, StartIsKeptWhenTimeAwayIsLeftOut)
{
	const QDateTime now = QDateTime::currentDateTime();
	ASSERT_TRUE(Database::saveTrackingState({ { "issue_id", 7 }, { "started_at", now.addSecs(-3 * 3600) },
		{ "elapsed_seconds", 600 }, { "tracking_started_at", now.addSecs(-4 * 3600) } }));
	QSqlQuery query;
	query.prepare("UPDATE tracking_state SET last_seen = ?");
	query.addBindValue(now.addSecs(-2 * 3600));
	ASSERT_TRUE(query.exec());

	TimeTracker restored;
	EXPECT_EQ(restored.startedAt(), now.addSecs(-4 * 3600));
	restored.resolveInterruption(false);
	EXPECT_EQ(restored.startedAt(), now.addSecs(-4 * 3600));
	EXPECT_EQ(restored.elapsedTime().toStdString(), "01:10:00");
}

TEST_F(TimeTrackerTest, StartCannotBeChangedWhileTheTimeAwayIsUndecided)
{
	const QDateTime now = QDateTime::currentDateTime();
	ASSERT_TRUE(Database::saveTrackingState({ { "issue_id", 7 }, { "started_at", now.addSecs(-5 * 3600) },
		{ "elapsed_seconds", 0 }, { "tracking_started_at", now.addSecs(-5 * 3600) } }));
	QSqlQuery query;
	query.prepare("UPDATE tracking_state SET last_seen = ?");
	query.addBindValue(now.addSecs(-4 * 3600));
	ASSERT_TRUE(query.exec());

	TimeTracker restored;
	ASSERT_TRUE(restored.interruptedAt().isValid());
	EXPECT_FALSE(restored.setStartedAt(now.addSecs(-3600)));
	EXPECT_EQ(restored.startedAt(), now.addSecs(-5 * 3600));

	restored.resolveInterruption(false);
	EXPECT_EQ(restored.elapsedTime().toStdString(), "01:00:00");
	EXPECT_TRUE(restored.setStartedAt(now.addSecs(-6 * 3600)));
	EXPECT_EQ(restored.elapsedTime().toStdString(), "02:00:00");
}

TEST_F(TimeTrackerTest, MigratedStartIgnoresTheTimeAway)
{
	const QDateTime now = QDateTime::currentDateTime();
	ASSERT_TRUE(Database::saveTrackingState(
		{ { "issue_id", 7 }, { "started_at", now.addSecs(-5 * 3600) }, { "elapsed_seconds", 600 } }));
	QSqlQuery query;
	query.prepare("UPDATE tracking_state SET last_seen = ?");
	query.addBindValue(now.addSecs(-4 * 3600));
	ASSERT_TRUE(query.exec());

	TimeTracker restored;
	ASSERT_TRUE(restored.interruptedAt().isValid());
	// 600 s plus the hour up to when the app was last seen, ending there
	EXPECT_EQ(restored.startedAt(), now.addSecs(-4 * 3600 - 4200));
}

TEST_F(TimeTrackerTest, StoppingClearsTheStart)
{
	tracker->startTracking(5);
	SignalCounter spy(tracker.get(), &TimeTracker::startedAtChanged);
	tracker->stopTracking();
	EXPECT_EQ(spy.count(), 1);
	EXPECT_FALSE(tracker->startedAt().isValid());
	tracker->stopTracking();
	EXPECT_EQ(spy.count(), 1);
}

TEST_F(TimeTrackerTest, TrackerSavedWithoutStartGetsOneFromItsTime)
{
	ASSERT_TRUE(
		Database::saveTrackingState({ { "issue_id", 7 }, { "started_at", QDateTime() }, { "elapsed_seconds", 5400 } }));
	TimeTracker restored;
	EXPECT_NEAR(restored.startedAt().secsTo(QDateTime::currentDateTime()), 5400, 2);
}

TEST_F(TimeTrackerTest, ClearingForgetsTheTrackerButKeepsTheActivity)
{
	tracker->setTrackingActivityId(9);
	tracker->setTrackingComment("Done");
	tracker->startTracking(5);
	ASSERT_TRUE(tracker->setStartedAt(QDateTime::currentDateTime().addSecs(-3600)));
	SignalCounter cleared(tracker.get(), &TimeTracker::trackingCleared);
	SignalCounter issue(tracker.get(), &TimeTracker::currentIssueChanged);
	SignalCounter details(tracker.get(), &TimeTracker::trackingDetailsChanged);
	SignalCounter start(tracker.get(), &TimeTracker::startedAtChanged);
	SignalCounter tracking(tracker.get(), &TimeTracker::trackingChanged);

	tracker->clearTracking();

	EXPECT_EQ(cleared.count(), 1);
	EXPECT_EQ(issue.count(), 1);
	EXPECT_EQ(details.count(), 1);
	EXPECT_EQ(start.count(), 1);
	EXPECT_EQ(tracking.count(), 1);
	EXPECT_FALSE(tracker->isTracking());
	EXPECT_EQ(tracker->currentIssueId(), 0);
	EXPECT_TRUE(tracker->trackingComment().isEmpty());
	EXPECT_FALSE(tracker->startedAt().isValid());
	EXPECT_EQ(tracker->trackingActivityId(), 9);
	EXPECT_EQ(tracker->elapsedTime().toStdString(), "00:00:00");
	EXPECT_DOUBLE_EQ(tracker->getElapsedHours(), 0.0);

	TimeTracker restarted;
	EXPECT_FALSE(restarted.isTracking());
	EXPECT_EQ(restarted.currentIssueId(), 0);
}

TEST_F(TimeTrackerTest, ClearingAfterStoppingWorks)
{
	tracker->startTracking(5);
	tracker->stopTracking();
	tracker->clearTracking();
	EXPECT_EQ(tracker->currentIssueId(), 0);
	EXPECT_FALSE(tracker->startedAt().isValid());
}
