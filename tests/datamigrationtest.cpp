#include "datamigration.h"
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <gtest/gtest.h>

using namespace Stride::DataMigration;

namespace {
void writeFile(const QString& path, const QByteArray& content)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	ASSERT_TRUE(file.open(QIODevice::WriteOnly));
	file.write(content);
}

QByteArray readFile(const QString& path)
{
	QFile file(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
}
}

class DataMigrationTest : public ::testing::Test {
protected:
	QString oldDir() const { return mRoot.filePath(QStringLiteral("old/QRedmine/Stride")); }

	QString newDir() const { return mRoot.filePath(QStringLiteral("new/Stride")); }

	QString inOld(const QString& name) const { return QDir(oldDir()).absoluteFilePath(name); }

	QString inNew(const QString& name) const { return QDir(newDir()).absoluteFilePath(name); }

	QTemporaryDir mRoot;
};

TEST_F(DataMigrationTest, LegacyDirectoryHasTheOrganizationInBetween)
{
	EXPECT_EQ(legacyDirectory(QStringLiteral("/home/u/.local/share/Stride")).toStdString(),
		"/home/u/.local/share/QRedmine/Stride");
	EXPECT_TRUE(legacyDirectory(QStringLiteral("/data/user/0/app/files")).isEmpty());
}

TEST_F(DataMigrationTest, SettingsAreCopiedAndTheOldOnesKept)
{
	QSettings from(mRoot.filePath(QStringLiteral("from.ini")), QSettings::IniFormat);
	from.setValue(QStringLiteral("client/baseUrl"), QStringLiteral("https://redmine.example"));
	from.setValue(QStringLiteral("ui/language"), QStringLiteral("de"));
	QSettings to(mRoot.filePath(QStringLiteral("to.ini")), QSettings::IniFormat);

	EXPECT_TRUE(migrateSettings(from, to));
	EXPECT_EQ(to.value(QStringLiteral("client/baseUrl")).toString().toStdString(), "https://redmine.example");
	EXPECT_EQ(to.value(QStringLiteral("ui/language")).toString().toStdString(), "de");
	EXPECT_EQ(from.allKeys().size(), 2);
}

TEST_F(DataMigrationTest, ExistingSettingsAreNeverOverwritten)
{
	QSettings from(mRoot.filePath(QStringLiteral("from.ini")), QSettings::IniFormat);
	from.setValue(QStringLiteral("ui/language"), QStringLiteral("de"));
	QSettings to(mRoot.filePath(QStringLiteral("to.ini")), QSettings::IniFormat);
	to.setValue(QStringLiteral("ui/language"), QStringLiteral("en"));

	EXPECT_FALSE(migrateSettings(from, to));
	EXPECT_EQ(to.value(QStringLiteral("ui/language")).toString().toStdString(), "en");
}

TEST_F(DataMigrationTest, NothingToMigrateWithoutOldSettings)
{
	QSettings from(mRoot.filePath(QStringLiteral("from.ini")), QSettings::IniFormat);
	QSettings to(mRoot.filePath(QStringLiteral("to.ini")), QSettings::IniFormat);
	EXPECT_FALSE(migrateSettings(from, to));
	EXPECT_TRUE(to.allKeys().isEmpty());
}

TEST_F(DataMigrationTest, DatabaseWithItsWriteAheadLogAndLogsAreCopied)
{
	writeFile(inOld(QStringLiteral("timeentries.db")), "db");
	writeFile(inOld(QStringLiteral("timeentries.db-wal")), "wal");
	writeFile(inOld(QStringLiteral("timeentries.db-shm")), "shm");
	writeFile(inOld(QStringLiteral("logs/stride.log")), "log");

	Report report;
	EXPECT_TRUE(migrateDirectory(oldDir(), newDir(), report));
	EXPECT_TRUE(report.warnings.isEmpty());
	EXPECT_EQ(readFile(inNew(QStringLiteral("timeentries.db"))).toStdString(), "db");
	EXPECT_EQ(readFile(inNew(QStringLiteral("timeentries.db-wal"))).toStdString(), "wal");
	EXPECT_EQ(readFile(inNew(QStringLiteral("timeentries.db-shm"))).toStdString(), "shm");
	EXPECT_EQ(readFile(inNew(QStringLiteral("logs/stride.log"))).toStdString(), "log");
	// Copied, not moved, and marked as done
	EXPECT_TRUE(QFile::exists(inOld(QStringLiteral("timeentries.db"))));
	EXPECT_TRUE(QFile::exists(inOld(MARKER_FILE_NAME)));
	EXPECT_FALSE(QFile::exists(inNew(MARKER_FILE_NAME)));
}

TEST_F(DataMigrationTest, ExistingDatabaseIsNeverOverwrittenNorGivenAnotherLog)
{
	writeFile(inOld(QStringLiteral("timeentries.db")), "old");
	writeFile(inOld(QStringLiteral("timeentries.db-wal")), "old-wal");
	writeFile(inNew(QStringLiteral("timeentries.db")), "new");

	Report report;
	migrateDirectory(oldDir(), newDir(), report);
	EXPECT_EQ(readFile(inNew(QStringLiteral("timeentries.db"))).toStdString(), "new");
	EXPECT_FALSE(QFile::exists(inNew(QStringLiteral("timeentries.db-wal"))));
	ASSERT_EQ(report.warnings.size(), 1);
	EXPECT_TRUE(report.warnings.first().contains(inNew(QStringLiteral("timeentries.db"))));
	EXPECT_TRUE(report.warnings.first().contains(inOld(QStringLiteral("timeentries.db"))));
	EXPECT_TRUE(QFile::exists(inOld(MARKER_FILE_NAME)));
}

TEST_F(DataMigrationTest, LegacySettingsOrganizationDependsOnThePlatform)
{
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
	EXPECT_EQ(legacySettingsOrganization().toStdString(), "drook207.gitlab.io");
#else
	EXPECT_EQ(legacySettingsOrganization().toStdString(), "QRedmine");
#endif
}

TEST_F(DataMigrationTest, ExistingFilesInSubdirectoriesAreKeptAndMissingOnesAdded)
{
	writeFile(inOld(QStringLiteral("logs/a.log")), "old-a");
	writeFile(inOld(QStringLiteral("logs/b.log")), "old-b");
	writeFile(inNew(QStringLiteral("logs/a.log")), "new-a");

	Report report;
	EXPECT_TRUE(migrateDirectory(oldDir(), newDir(), report));
	EXPECT_EQ(readFile(inNew(QStringLiteral("logs/a.log"))).toStdString(), "new-a");
	EXPECT_EQ(readFile(inNew(QStringLiteral("logs/b.log"))).toStdString(), "old-b");
}

TEST_F(DataMigrationTest, IsNotRepeatedOnceDone)
{
	writeFile(inOld(QStringLiteral("timeentries.db")), "db");
	Report report;
	ASSERT_TRUE(migrateDirectory(oldDir(), newDir(), report));

	// The user starts over at the new location, the old data isn't resurrected
	QFile::remove(inNew(QStringLiteral("timeentries.db")));
	EXPECT_FALSE(migrateDirectory(oldDir(), newDir(), report));
	EXPECT_FALSE(QFile::exists(inNew(QStringLiteral("timeentries.db"))));
}

TEST_F(DataMigrationTest, MissingOldDirectoryOrSameDirectoryDoNothing)
{
	Report report;
	EXPECT_FALSE(migrateDirectory(oldDir(), newDir(), report));
	EXPECT_FALSE(migrateDirectory(QString(), newDir(), report));
	writeFile(inNew(QStringLiteral("timeentries.db")), "db");
	EXPECT_FALSE(migrateDirectory(newDir(), newDir(), report));
	EXPECT_FALSE(QFile::exists(inNew(MARKER_FILE_NAME)));
}
