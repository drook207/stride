#include "database.h"
#include <QFile>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <gtest/gtest.h>

using QRedmineClient::Database;

class DatabaseTest : public ::testing::Test {
protected:
	void SetUp() override { ASSERT_TRUE(Database::initialize(QStringLiteral(":memory:"))); }

	void TearDown() override { Database::close(); }
};

TEST_F(DatabaseTest, NewDatabaseIsMigratedToTheLatestSchema)
{
	EXPECT_GE(Database::schemaVersion(), 1);
	EXPECT_TRUE(Database::getRecentEntries().isEmpty());
}

TEST_F(DatabaseTest, RecentEntriesAreNewestFirstWithIdAsTiebreaker)
{
	const QDateTime day(QDate(2026, 10, 1), QTime(0, 0));
	const int first = Database::addTimeEntry(1, 1.0, "first", day);
	const int second = Database::addTimeEntry(2, 2.0, "second", day);
	ASSERT_GT(second, first);

	const QJsonArray entries = Database::getRecentEntries();
	ASSERT_EQ(entries.size(), 2);
	// Saved within the same timestamp tick most likely; the higher id wins either way.
	EXPECT_EQ(entries[0].toObject()["id"].toInt(), second);
	EXPECT_EQ(entries[1].toObject()["comments"].toString().toStdString(), "first");
}

TEST_F(DatabaseTest, UpdatesReportHowManyEntriesChanged)
{
	const QDateTime day(QDate(2026, 10, 1), QTime(0, 0));
	const int id = Database::addTimeEntry(1, 1.0, "entry", day);
	ASSERT_TRUE(Database::updateSyncStatus(id, 77));

	EXPECT_EQ(Database::updateTimeEntry(id, 2.0, "changed", day), 1);
	EXPECT_EQ(Database::updateTimeEntry(id + 100, 2.0, "nothing", day), 0);
	EXPECT_EQ(Database::updateTimeEntriesByRedmineId(77, 3.0, "via redmine id", day), 1);
	EXPECT_EQ(Database::updateTimeEntriesByRedmineId(78, 3.0, "unknown", day), 0);
	EXPECT_EQ(Database::linkedRedmineIds(), QSet<int> { 77 });
	EXPECT_EQ(Database::getRecentEntries()[0].toObject()["comments"].toString().toStdString(), "via redmine id");
}

TEST(DatabaseMigrationTest, DatabaseFromBeforeVersioningGetsProjectNameAndAVersion)
{
	QTemporaryDir dir;
	ASSERT_TRUE(dir.isValid());
	const QString path = dir.filePath("legacy.db");
	{
		// The schema of the first releases: no user_version, issues without project_name.
		QSqlDatabase legacy = QSqlDatabase::addDatabase("QSQLITE", "legacy");
		legacy.setDatabaseName(path);
		ASSERT_TRUE(legacy.open());
		QSqlQuery query(legacy);
		ASSERT_TRUE(query.exec("CREATE TABLE issues (id INTEGER PRIMARY KEY NOT NULL, subject TEXT NOT NULL, "
							   "project_id INTEGER, status_id INTEGER NOT NULL, description TEXT, "
							   "created_on DATETIME NOT NULL, updated_on DATETIME NOT NULL)"));
		ASSERT_TRUE(query.exec("INSERT INTO issues VALUES (5, 'Old issue', 1, 1, '', '2026-01-01', '2026-01-01')"));
		legacy.close();
	}
	QSqlDatabase::removeDatabase("legacy");

	ASSERT_TRUE(Database::initialize(path));
	EXPECT_GE(Database::schemaVersion(), 1);
	EXPECT_TRUE(Database::addIssue(
		6, "New issue", 1, "Project", 1, "", QDateTime::currentDateTime(), QDateTime::currentDateTime()));
	QSqlQuery issues = Database::getIssues();
	int count = 0;
	while (issues.next()) {
		++count;
	}
	EXPECT_EQ(count, 2);
	Database::close();

	// Opening it again doesn't migrate anything twice.
	ASSERT_TRUE(Database::initialize(path));
	EXPECT_GE(Database::schemaVersion(), 1);
	Database::close();
}

TEST(DatabaseMoveTest, MovesTheDatabaseWithItsLatestDataOrLeavesItWhereItWas)
{
	QTemporaryDir dir;
	ASSERT_TRUE(dir.isValid());
	const QString from = dir.filePath("old.db");
	{
		QSqlDatabase legacy = QSqlDatabase::addDatabase("QSQLITE", "legacy");
		legacy.setDatabaseName(from);
		ASSERT_TRUE(legacy.open());
		QSqlQuery query(legacy);
		ASSERT_TRUE(query.exec("PRAGMA journal_mode=WAL"));
		ASSERT_TRUE(query.exec("CREATE TABLE t (x)"));
		ASSERT_TRUE(query.exec("INSERT INTO t VALUES (42)"));
		legacy.close();
	}
	QSqlDatabase::removeDatabase("legacy");

	// A target that can't be written: nothing changes.
	EXPECT_FALSE(Database::moveDatabaseFiles(from, dir.filePath("missing/dir/new.db")));
	EXPECT_TRUE(QFile::exists(from));

	const QString to = dir.filePath("new.db");
	ASSERT_TRUE(Database::moveDatabaseFiles(from, to));
	EXPECT_FALSE(QFile::exists(from));
	{
		QSqlDatabase moved = QSqlDatabase::addDatabase("QSQLITE", "moved");
		moved.setDatabaseName(to);
		ASSERT_TRUE(moved.open());
		QSqlQuery query(moved);
		ASSERT_TRUE(query.exec("SELECT x FROM t"));
		ASSERT_TRUE(query.next());
		EXPECT_EQ(query.value(0).toInt(), 42);
		moved.close();
	}
	QSqlDatabase::removeDatabase("moved");
}
