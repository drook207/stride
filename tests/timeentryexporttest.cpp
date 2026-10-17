#include "timeentryexport.h"
#include "database.h"
#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <gtest/gtest.h>

using QRedmineClient::Database;
using QRedmineClient::TimeEntryExport;

class TimeEntryExportTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		ASSERT_TRUE(Database::initialize(QStringLiteral(":memory:")));
		QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
	}

	void TearDown() override
	{
		QLocale::setDefault(QLocale::c());
		Database::close();
	}

	const QDate from { 2026, 10, 12 };
	const QDate to { 2026, 10, 18 };

	static QJsonObject remote(int id, const QString& day, double hours, const QString& comments)
	{
		return { { "id", id }, { "spent_on", day }, { "hours", hours }, { "comments", comments },
			{ "issue", QJsonObject { { "id", 5 } } }, { "project", QJsonObject { { "name", "Website" } } },
			{ "activity", QJsonObject { { "name", "Development" } } } };
	}

	QTemporaryDir dir;
	TimeEntryExport exporter;
};

TEST_F(TimeEntryExportTest, RowsCombineRedmineEntriesWithLocalOnesNotSentYet)
{
	const int synced = Database::addTimeEntry(5, 2.0, "On Redmine", QDateTime(from, QTime()));
	Database::updateSyncStatus(synced, 100);
	Database::addTimeEntry(6, 1.0, "Pending", QDateTime(from.addDays(1), QTime()));

	const QVariantList rows = exporter.rows(QJsonArray { remote(100, "2026-10-12", 2.0, "On Redmine"),
												remote(101, "2026-10-20", 3.0, "Outside the period") },
		true, from, to);

	ASSERT_EQ(rows.size(), 2);
	EXPECT_EQ(rows[0].toMap()["activity"].toString().toStdString(), "Development");
	EXPECT_EQ(rows[1].toMap()["comments"].toString().toStdString(), "Pending");
	EXPECT_TRUE(rows[1].toMap()["pending"].toBool());
}

TEST_F(TimeEntryExportTest, CsvUsesTheLocalesSeparatorsAndQuotesWhereNeeded)
{
	const QVariantList rows =
		exporter.rows(QJsonArray { remote(100, "2026-10-12", 1.5, "Fixed \"login\"; tested") }, true, from, to);
	const QString path = dir.filePath("entries.csv");
	ASSERT_TRUE(exporter.writeCsv(QUrl::fromLocalFile(path), rows));

	QFile file(path);
	ASSERT_TRUE(file.open(QIODevice::ReadOnly));
	const QByteArray content = file.readAll();
	EXPECT_TRUE(content.startsWith("\xEF\xBB\xBF"));
	EXPECT_TRUE(content.contains("2026-10-12;5;Website;Development;1,50;\"Fixed \"\"login\"\"; tested\"\r\n"))
		<< content.toStdString();
}

TEST_F(TimeEntryExportTest, EntriesWithoutAnIssueHaveAnEmptyIssueColumn)
{
	QJsonObject meeting = remote(7, "2026-10-12", 1.0, "Planning");
	meeting.remove("issue");
	const QString path = dir.filePath("meeting.csv");
	ASSERT_TRUE(exporter.writeCsv(QUrl::fromLocalFile(path), exporter.rows(QJsonArray { meeting }, true, from, to)));

	QFile file(path);
	ASSERT_TRUE(file.open(QIODevice::ReadOnly));
	EXPECT_TRUE(file.readAll().contains("2026-10-12;;Website;"));
}

TEST_F(TimeEntryExportTest, PdfIsWritten)
{
	const QString path = dir.filePath("entries.pdf");
	ASSERT_TRUE(exporter.writePdf(QUrl::fromLocalFile(path),
		exporter.rows(QJsonArray { remote(1, "2026-10-13", 8, "") }, true, from, to), "Week 42"));

	QFile file(path);
	ASSERT_TRUE(file.open(QIODevice::ReadOnly));
	EXPECT_TRUE(file.read(4) == "%PDF");
}

TEST_F(TimeEntryExportTest, UnwritableFileIsReported)
{
	EXPECT_FALSE(exporter.writeCsv(QUrl::fromLocalFile(dir.filePath("missing/dir/entries.csv")), {}));
	EXPECT_FALSE(exporter.lastError().isEmpty());
}
