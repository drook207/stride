#include "timeoverview.h"
#include "database.h"
#include <QJsonObject>
#include <QSettings>
#include <gtest/gtest.h>

using QRedmineClient::Database;
using QRedmineClient::TimeOverview;

class TimeOverviewTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		QSettings().clear();
		ASSERT_TRUE(Database::initialize(QStringLiteral(":memory:")));
	}

	void TearDown() override
	{
		Database::close();
		QSettings().clear();
	}

	// A Monday some weeks back, so every day of its week is in the past.
	const QDate monday = QDate::currentDate().addDays(1 - QDate::currentDate().dayOfWeek() - 14);

	static QJsonObject remote(int id, const QDate& day, double hours, const QString& project)
	{
		return { { "id", id }, { "spent_on", day.toString(Qt::ISODate) }, { "hours", hours },
			{ "project", QJsonObject { { "name", project } } } };
	}

	static double hoursOf(const TimeOverview& overview, int weekday)
	{
		return overview.days().at(weekday).toMap()["hours"].toDouble();
	}
};

TEST_F(TimeOverviewTest, CountsLocalEntriesUntilRedmineAnswers)
{
	Database::addTimeEntry(1, 2.0, "", QDateTime(monday, QTime()));
	Database::addTimeEntry(1, 1.5, "", QDateTime(monday.addDays(2), QTime()));
	Database::addTimeEntry(1, 9.0, "", QDateTime(monday.addDays(7), QTime())); // next week

	TimeOverview overview;
	overview.showWeekOf(monday.addDays(3));

	EXPECT_EQ(overview.weekStart(), monday);
	EXPECT_DOUBLE_EQ(overview.totalHours(), 3.5);
	EXPECT_DOUBLE_EQ(hoursOf(overview, 0), 2.0);
	EXPECT_DOUBLE_EQ(hoursOf(overview, 2), 1.5);
	EXPECT_FALSE(overview.includesRedmine());
	// A past week: 5 workdays of 8 h, 3.5 h booked.
	EXPECT_DOUBLE_EQ(overview.missingHours(), 36.5);
}

TEST_F(TimeOverviewTest, RedmineEntriesReplaceLocalOnesExceptThoseNotSentYet)
{
	const int sent = Database::addTimeEntry(1, 2.0, "", QDateTime(monday, QTime()));
	Database::updateSyncStatus(sent, 100);
	Database::addTimeEntry(1, 1.0, "", QDateTime(monday, QTime())); // pending

	TimeOverview overview;
	overview.showWeekOf(monday);
	const QVariantMap week = overview.range();
	overview.setRemoteEntries(
		QJsonArray { remote(100, monday, 2.0, "Website"), remote(101, monday.addDays(1), 4.0, "Mobile App") },
		week["from"].toString(), week["to"].toString());

	EXPECT_TRUE(overview.includesRedmine());
	EXPECT_DOUBLE_EQ(hoursOf(overview, 0), 3.0); // Redmine's 2 h plus the pending 1 h, not 2 h twice
	EXPECT_DOUBLE_EQ(hoursOf(overview, 1), 4.0);
	ASSERT_EQ(overview.projects().size(), 3);
	EXPECT_EQ(overview.projects().first().toMap()["name"].toString().toStdString(), "Mobile App");
}

TEST_F(TimeOverviewTest, IgnoresRedmineEntriesForAnotherWeek)
{
	TimeOverview overview;
	overview.showWeekOf(monday);
	overview.setRemoteEntries(QJsonArray { remote(1, monday.addDays(7), 5.0, "X") },
		monday.addDays(7).toString(Qt::ISODate), monday.addDays(13).toString(Qt::ISODate));

	EXPECT_FALSE(overview.includesRedmine());
	EXPECT_DOUBLE_EQ(overview.totalHours(), 0);
}

TEST_F(TimeOverviewTest, DailyTargetIsStoredAndCountsOnWorkdaysOnly)
{
	{
		TimeOverview overview;
		overview.setDailyTarget(6);
	}
	TimeOverview overview;
	overview.showWeekOf(monday);
	EXPECT_DOUBLE_EQ(overview.dailyTarget(), 6);
	EXPECT_DOUBLE_EQ(overview.targetHours(), 30);
	EXPECT_DOUBLE_EQ(overview.days().at(5).toMap()["target"].toDouble(), 0); // Saturday
}
