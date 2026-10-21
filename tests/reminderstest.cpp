#include "reminders.h"
#include "database.h"
#include "mockhttpserver.h"
#include "redmineclient.h"
#include "testutils.h"
#include "timetracker.h"
#include <QSettings>
#include <gtest/gtest.h>

using Stride::Database;
using Stride::Reminders;
using Stride::TimeTracker;

class RemindersTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		QSettings().clear();
		ASSERT_TRUE(Database::initialize(QStringLiteral(":memory:")));
		tracker = std::make_unique<TimeTracker>();
		reminders = std::make_unique<Reminders>();
		reminders->setTimeTracker(tracker.get());
		QObject::connect(reminders.get(), &Reminders::reminder,
			[this](const QString& title, const QString&) { titles.push_back(title.toStdString()); });
	}

	void TearDown() override
	{
		reminders.reset();
		tracker.reset();
		Database::close();
		QSettings().clear();
	}

	// A Wednesday, well after the default end of day.
	const QDateTime wednesdayEvening { QDate(2026, 10, 14), QTime(18, 0) };

	std::unique_ptr<TimeTracker> tracker;
	std::unique_ptr<Reminders> reminders;
	std::vector<std::string> titles;
};

TEST_F(RemindersTest, RemindsOnceOfAWorkdayWithoutBookedTime)
{
	reminders->check(wednesdayEvening.addSecs(-3 * 3600)); // before the end of the day
	EXPECT_TRUE(titles.empty());

	reminders->check(wednesdayEvening);
	reminders->check(wednesdayEvening.addSecs(600));
	EXPECT_EQ(titles, std::vector<std::string> { "Nothing booked today" });

	// Not on the weekend.
	reminders->check(QDateTime(QDate(2026, 10, 17), QTime(18, 0)));
	EXPECT_EQ(titles.size(), 1u);
}

TEST_F(RemindersTest, NoReminderWithTimeBookedOrWhenDisabled)
{
	Database::addTimeEntry(1, 2.0, "", QDateTime(wednesdayEvening.date(), QTime()));
	reminders->check(wednesdayEvening);
	EXPECT_TRUE(titles.empty());

	reminders->setEnabled(false);
	reminders->check(wednesdayEvening.addDays(1));
	EXPECT_TRUE(titles.empty());
	EXPECT_FALSE(Reminders().enabled()); // stored
}

TEST_F(RemindersTest, LongRunningTimerIsRemindedOncePerStretch)
{
	reminders->setLongTrackingHours(1);
	ASSERT_TRUE(Database::saveTrackingState({ { "issue_id", 42 },
		{ "started_at", QDateTime::currentDateTime().addSecs(-3700) }, { "elapsed_seconds", 0 } }));
	TimeTracker running; // restored: running for just over an hour
	reminders->setTimeTracker(&running);

	const QDateTime morning(QDate(2026, 10, 14), QTime(9, 0));
	reminders->check(morning);
	reminders->check(morning.addSecs(60));
	EXPECT_EQ(titles, std::vector<std::string> { "Timer still running" });
	reminders->setTimeTracker(nullptr);
}

class RemindersWithRedmineTest : public RemindersTest {
protected:
	void SetUp() override
	{
		RemindersTest::SetUp();
		client = std::make_unique<QRedmineClient::RedmineClient>();
		client->setBaseUrl(server.baseUrl());
		reminders->setClient(client.get());
	}

	void TearDown() override
	{
		reminders->setClient(nullptr);
		client.reset();
		RemindersTest::TearDown();
	}

	QRedmineTesting::MockHttpServer server;
	std::unique_ptr<QRedmineClient::RedmineClient> client;
};

TEST_F(RemindersWithRedmineTest, TimeBookedOnRedmineCounts)
{
	server.enqueueResponse(QRedmineTesting::MockHttpResponse {
		200, "OK", R"({"time_entries":[{"id":1,"hours":8}],"total_count":1,"offset":0})" });

	reminders->check(wednesdayEvening);
	QRedmineTesting::waitUntil([]() { return false; }, 300);

	EXPECT_TRUE(titles.empty());
}

TEST_F(RemindersWithRedmineTest, RemindsWhenRedmineHasNothingEitherOrCantBeAsked)
{
	server.enqueueResponse(
		QRedmineTesting::MockHttpResponse { 200, "OK", R"({"time_entries":[],"total_count":0,"offset":0})" });
	reminders->check(wednesdayEvening);
	ASSERT_TRUE(QRedmineTesting::waitUntil([this]() { return titles.size() == 1; }));

	server.enqueueResponse(QRedmineTesting::MockHttpResponse { 500, "Internal Server Error", "" });
	reminders->check(wednesdayEvening.addDays(1));
	ASSERT_TRUE(QRedmineTesting::waitUntil([this]() { return titles.size() == 2; }));
}
