#include "issuemodel.h"
#include "database.h"
#include <QDateTime>
#include <gtest/gtest.h>

using Stride::Database;
using Stride::IssueModel;

class IssueModelTest : public ::testing::Test {
protected:
	void SetUp() override { ASSERT_TRUE(Database::initialize(QStringLiteral(":memory:"))); }

	void TearDown() override { Database::close(); }

	//! Adds a cached issue with the given subject.
	static void addIssue(int id, const QString& subject)
	{
		const QDateTime now = QDateTime::currentDateTime();
		ASSERT_TRUE(Database::addIssue(id, subject, 1, QStringLiteral("Project"), 1, QString(), now, now));
	}
};

TEST_F(IssueModelTest, ListsTheCachedIssuesInOneFlatList)
{
	addIssue(1, QStringLiteral("First"));
	addIssue(2, QStringLiteral("Second"));

	IssueModel model;
	EXPECT_EQ(model.rowCount(), 2);
	EXPECT_TRUE(model.hasIssue(2));
	EXPECT_FALSE(model.hasIssue(3));
}

TEST_F(IssueModelTest, HasNoRowsBelowAValidParentIndex)
{
	addIssue(1, QStringLiteral("First"));

	IssueModel model;
	ASSERT_EQ(model.rowCount(), 1);
	// A flat list: the rows hang off the invisible root only, never below an issue of their own.
	EXPECT_EQ(model.rowCount(model.index(0, 0)), 0);
}
