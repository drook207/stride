#include "timetracker.h"
#include "database.h"
#include <QJsonDocument>
#include <gtest/gtest.h>

using QRedmineClient::Database;
using QRedmineClient::TimeTracker;

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

	// Not reached: stays pending and is sent again later.
	tracker->submissionFailed(local, 0, "offline");
	EXPECT_EQ(tracker->recentEntries().first().toMap()["sync_status"].toInt(), Database::Pending);
	tracker->submitPendingEntries();
	EXPECT_EQ(sent.size(), 2u);

	EXPECT_FALSE(tracker->linkCreatedTimeEntry(remote(900, 10, 1.0, "Offline"), local)); // background
	EXPECT_EQ(redmineIdOf(local), 900);
	tracker->submitPendingEntries();
	EXPECT_EQ(sent.size(), 2u);
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
