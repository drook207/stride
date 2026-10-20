#include "logtest.h"
#include "testutils.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <gtest/gtest.h>
#include <thread>

#include "logging.h"

using QRedmineClient::Utils::Logging::LogEntry;
using QRedmineClient::Utils::Logging::LogManager;
using QRedmineTesting::waitUntil;

namespace {
Q_LOGGING_CATEGORY(lcTest, "qredmine.test")
Q_LOGGING_CATEGORY(lcNetwork, "qredmine.client.network")
Q_LOGGING_CATEGORY(lcTracker, "qredmine.timetracker")
Q_LOGGING_CATEGORY(lcOther, "other.test")

const QString UNICODE_MESSAGE = QStringLiteral("Grüße \"quoted\" \\ back\nnew line\ttab € \U0001F600");
}

TEST_F(LogTest, WritesOneValidJsonObjectPerLineWithTheBasicKeys)
{
	createManager();
	QR_INFO(lcTest, QStringLiteral("first"));
	QR_INFO(lcTest, QStringLiteral("second"));

	const QList<QJsonObject> entries = readLog();
	ASSERT_EQ(entries.size(), 2);
	const QJsonObject& entry = entries.first();
	EXPECT_EQ(entry.value("level").toString().toStdString(), "info");
	EXPECT_EQ(entry.value("cat").toString().toStdString(), "qredmine.test");
	EXPECT_EQ(entry.value("msg").toString().toStdString(), "first");
	EXPECT_TRUE(QDateTime::fromString(entry.value("ts").toString(), Qt::ISODateWithMs).isValid());
	EXPECT_FALSE(entry.contains("fields"));
	EXPECT_EQ(entries.last().value("msg").toString().toStdString(), "second");
}

TEST_F(LogTest, WritesStructuredFieldsUnderFields)
{
	createManager();
	QR_INFO(lcTest, QStringLiteral("with fields"), { { "id", 12 }, { "name", "x" }, { "ok", true } });

	const QJsonObject fields = findInLog(QStringLiteral("with fields")).value("fields").toObject();
	EXPECT_EQ(fields.value("id").toInt(), 12);
	EXPECT_EQ(fields.value("name").toString().toStdString(), "x");
	EXPECT_TRUE(fields.value("ok").toBool());
}

TEST_F(LogTest, FileAndLineOnlyForWarningsAndAbove)
{
	createManager();
	QR_INFO(lcTest, QStringLiteral("an info"));
	QR_WARNING(lcTest, QStringLiteral("a warning"));

	const QJsonObject info = findInLog(QStringLiteral("an info"));
	const QJsonObject warning = findInLog(QStringLiteral("a warning"));
	EXPECT_FALSE(info.contains("file"));
	EXPECT_FALSE(info.contains("line"));
	EXPECT_EQ(warning.value("level").toString().toStdString(), "warning");
	EXPECT_TRUE(warning.value("file").toString().contains(QStringLiteral("logmanagertest")));
	EXPECT_GT(warning.value("line").toInt(), 0);
}

TEST_F(LogTest, EscapesUnicodeQuotesAndNewlines)
{
	createManager();
	QR_INFO(lcTest, UNICODE_MESSAGE, { { "text", UNICODE_MESSAGE } });

	// One physical line despite the newline in the message
	EXPECT_EQ(readRaw().count('\n'), 1);
	const QJsonObject entry = readLog().value(0);
	EXPECT_EQ(entry.value("msg").toString(), UNICODE_MESSAGE);
	EXPECT_EQ(entry.value("fields").toObject().value("text").toString(), UNICODE_MESSAGE);
}

TEST_F(LogTest, ToJsonLineMatchesTheEntryAndEndsWithANewline)
{
	LogEntry entry;
	entry.timestamp = QDateTime::fromString(QStringLiteral("2026-10-20T14:32:11.123+02:00"), Qt::ISODateWithMs);
	entry.type = QtCriticalMsg;
	entry.category = QStringLiteral("cat");
	entry.message = QStringLiteral("msg");
	entry.fields = { { "k", 1 } };
	entry.file = QStringLiteral("f.cpp");
	entry.line = 7;

	const QByteArray line = LogManager::toJsonLine(entry);
	ASSERT_TRUE(line.endsWith('\n'));
	const QJsonObject object = QJsonDocument::fromJson(line).object();
	EXPECT_EQ(object.value("ts").toString().toStdString(), "2026-10-20T14:32:11.123+02:00");
	EXPECT_EQ(object.value("level").toString().toStdString(), "critical");
	EXPECT_EQ(object.value("fields").toObject().value("k").toInt(), 1);
	EXPECT_EQ(object.value("file").toString().toStdString(), "f.cpp");
	EXPECT_EQ(object.value("line").toInt(), 7);
	EXPECT_TRUE(line.startsWith(R"({"ts":)")); // the order is part of the format
}

TEST_F(LogTest, FieldsDoNotLeakIntoTheNextMessage)
{
	createManager();
	QR_INFO(lcTest, QStringLiteral("with"), { { "k", 1 } });
	QR_INFO(lcTest, QStringLiteral("without"));
	qCInfo(lcTest) << "plain";

	EXPECT_TRUE(findInLog(QStringLiteral("with")).contains("fields"));
	EXPECT_FALSE(findInLog(QStringLiteral("without")).contains("fields"));
	EXPECT_FALSE(findInLog(QStringLiteral("plain")).contains("fields"));
}

TEST_F(LogTest, FieldsOfAFilteredMessageDoNotLeakEither)
{
	createManager();
	ASSERT_FALSE(lcTest().isDebugEnabled());
	QR_DEBUG(lcTest, QStringLiteral("filtered"), { { "k", 1 } });
	QR_INFO(lcTest, QStringLiteral("next"));

	EXPECT_FALSE(findInLog(QStringLiteral("next")).contains("fields"));
}

TEST_F(LogTest, RecordsMessagesOfOtherThreads)
{
	createManager();
	std::thread worker([] {
		for (int i = 0; i < 20; ++i) {
			QR_INFO(lcTest, QStringLiteral("worker %1").arg(i), { { "i", i } });
		}
		QR_INFO(lcTest, QStringLiteral("worker without fields"));
	});
	for (int i = 0; i < 20; ++i) {
		QR_INFO(lcTest, QStringLiteral("main %1").arg(i));
	}
	worker.join();

	const QList<QJsonObject> entries = readLog();
	EXPECT_EQ(entries.size(), 41);
	int workerEntries = 0;
	for (const QJsonObject& entry : entries) {
		const QString message = entry.value("msg").toString();
		if (message.startsWith(QStringLiteral("worker "))) {
			++workerEntries;
		}
		// Fields never move over to a message of another thread
		EXPECT_EQ(entry.contains("fields"), message.startsWith(QStringLiteral("worker ")) && message.size() < 12)
			<< message.toStdString();
	}
	EXPECT_EQ(workerEntries, 21);
	EXPECT_EQ(manager->bufferedEntries().size(), 41);
}

TEST_F(LogTest, BufferKeepsOnlyTheLatestEntries)
{
	createManager();
	const int total = LogManager::MAX_BUFFERED_ENTRIES + 100;
	for (int i = 0; i < total; ++i) {
		QR_INFO(lcTest, QString::number(i));
	}

	const QList<LogEntry> buffered = manager->bufferedEntries();
	ASSERT_EQ(buffered.size(), LogManager::MAX_BUFFERED_ENTRIES);
	EXPECT_EQ(buffered.first().message.toStdString(), "100");
	EXPECT_EQ(buffered.last().message.toStdString(), QByteArray::number(total - 1).toStdString());
	for (qsizetype i = 1; i < buffered.size(); ++i) {
		ASSERT_LT(buffered.at(i - 1).sequence, buffered.at(i).sequence);
	}
	// The file has everything
	EXPECT_EQ(readLog().size(), total);
}

TEST_F(LogTest, EmitsEntryLoggedForEveryMessage)
{
	createManager();
	QStringList seen;
	QObject::connect(manager.get(), &LogManager::entryLogged, manager.get(),
		[&seen](const LogEntry& entry) { seen << entry.message; });
	QR_INFO(lcTest, QStringLiteral("a"));
	std::thread([] { QR_INFO(lcTest, QStringLiteral("b")); }).join();

	ASSERT_TRUE(waitUntil([&] { return seen.size() == 2; }));
	EXPECT_TRUE(seen.contains(QStringLiteral("a")));
	EXPECT_TRUE(seen.contains(QStringLiteral("b")));
}

TEST_F(LogTest, PassesMessagesOnToThePreviousHandlerAndRestoresItAfterwards)
{
	static QStringList passedOn;
	passedOn.clear();
	// Replace the fixture's handler by one that records
	qInstallMessageHandler(+[](QtMsgType, const QMessageLogContext&, const QString& message) { passedOn << message; });
	createManager();
	QR_INFO(lcTest, QStringLiteral("hello"));
	EXPECT_TRUE(passedOn.contains(QStringLiteral("hello")));

	destroyManager();
	passedOn.clear();
	QR_INFO(lcTest, QStringLiteral("after"));
	// The previous handler is back, so the message is no longer recorded, but still arrives
	EXPECT_TRUE(passedOn.contains(QStringLiteral("after")));
	EXPECT_EQ(LogManager::instance(), nullptr);
	EXPECT_FALSE(readRaw().contains("after"));
}

TEST_F(LogTest, RotatesAtTheSizeLimitAndKeepsTheNewestInTheCurrentFile)
{
	createManager();
	const int messageSize = 1024 * 1024;
	const int total = 33; // 4 per file: 8 files worth, more than are kept
	for (int i = 0; i < total; ++i) {
		QR_INFO(
			lcTest, QStringLiteral("#%1 ").arg(i, 3, 10, QLatin1Char('0')) + QString(messageSize, QLatin1Char('x')));
	}
	manager->flush();

	EXPECT_TRUE(QFileInfo::exists(path(QStringLiteral("stride.1.log"))));
	const QStringList files = manager->logFiles();
	EXPECT_EQ(files.size(), LogManager::MAX_FILES);
	EXPECT_FALSE(QFileInfo::exists(path(QStringLiteral("stride.5.log"))));
	for (const QString& file : files) {
		EXPECT_LE(QFileInfo(file).size(), LogManager::MAX_FILE_SIZE) << file.toStdString();
	}
	// Oldest first, the last one is stride.log; it has the newest message
	EXPECT_EQ(files.last(), path(QStringLiteral("stride.log")));
	EXPECT_TRUE(readRaw().contains("#032 "));
	EXPECT_FALSE(readRaw(QStringLiteral("stride.1.log")).contains("#032 "));
	// The files are in age order: the newest message of an older file is older than any of a newer one
	EXPECT_TRUE(readRaw(QStringLiteral("stride.1.log")).contains("#031 ") ||
		readRaw(QStringLiteral("stride.1.log")).contains("#028 "));
	// Whole lines only, so every file is valid
	for (const QString& file : files) {
		EXPECT_FALSE(readLog(QFileInfo(file).fileName()).isEmpty());
	}
}

TEST_F(LogTest, AppendsToTheLogFileOfAPreviousRun)
{
	createManager();
	QR_INFO(lcTest, QStringLiteral("run one"));
	destroyManager();
	createManager();
	QR_INFO(lcTest, QStringLiteral("run two"));

	const QList<QJsonObject> entries = readLog();
	ASSERT_EQ(entries.size(), 2);
	EXPECT_EQ(entries.first().value("msg").toString().toStdString(), "run one");
}

TEST_F(LogTest, NormalLevelLogsOnlyTheAppsInformation)
{
	createManager();
	EXPECT_EQ(manager->level(), LogManager::Normal);
	EXPECT_TRUE(lcTest().isInfoEnabled());
	EXPECT_FALSE(lcTest().isDebugEnabled());
	EXPECT_FALSE(lcNetwork().isDebugEnabled());
	EXPECT_FALSE(lcOther().isInfoEnabled());
	EXPECT_TRUE(lcOther().isWarningEnabled());
}

TEST_F(LogTest, VerboseLevelAddsNetworkAndTrackerDebug)
{
	createManager();
	manager->setLevel(LogManager::Verbose);
	EXPECT_TRUE(lcNetwork().isDebugEnabled());
	EXPECT_TRUE(lcTracker().isDebugEnabled());
	EXPECT_FALSE(lcTest().isDebugEnabled());
	EXPECT_FALSE(lcOther().isDebugEnabled());
}

TEST_F(LogTest, DebugLevelAddsTheAppsDebugOnly)
{
	createManager();
	manager->setLevel(LogManager::Debug);
	EXPECT_TRUE(lcTest().isDebugEnabled());
	EXPECT_TRUE(lcNetwork().isDebugEnabled());
	EXPECT_FALSE(lcOther().isDebugEnabled());
	EXPECT_FALSE(lcOther().isInfoEnabled());
}

TEST_F(LogTest, LoweringTheLevelSuppressesAgain)
{
	createManager();
	manager->setLevel(LogManager::Debug);
	manager->setLevel(LogManager::Normal);
	EXPECT_FALSE(lcTest().isDebugEnabled());
	EXPECT_FALSE(lcNetwork().isDebugEnabled());
}

TEST_F(LogTest, SetLevelPersistsAndNotifiesAndIsPickedUpByTheNextManager)
{
	createManager();
	int notifications = 0;
	QObject::connect(manager.get(), &LogManager::levelChanged, [&] { ++notifications; });
	manager->setLevel(LogManager::Debug);
	manager->setLevel(LogManager::Debug); // no change
	EXPECT_EQ(notifications, 1);
	EXPECT_EQ(QSettings().value(LogManager::LEVEL_SETTING).toString().toStdString(), "debug");
	EXPECT_EQ(LogManager::LEVEL_SETTING.toStdString(), "logging/level");

	destroyManager();
	createManager();
	EXPECT_EQ(manager->level(), LogManager::Debug);
	EXPECT_TRUE(lcTest().isDebugEnabled());
}

TEST_F(LogTest, UnknownPersistedLevelFallsBackToNormal)
{
	QSettings().setValue(LogManager::LEVEL_SETTING, QStringLiteral("loud"));
	createManager();
	EXPECT_EQ(manager->level(), LogManager::Normal);
	EXPECT_EQ(LogManager::levelFromName(QStringLiteral("verbose")), LogManager::Verbose);
	EXPECT_EQ(LogManager::levelName(LogManager::Debug).toStdString(), "debug");
}

TEST_F(LogTest, EnvironmentRulesWinOverTheLevel)
{
	qputenv("QT_LOGGING_RULES", "qredmine.test.debug=true;other.test.info=true");
	createManager();
	EXPECT_EQ(manager->level(), LogManager::Normal);
	EXPECT_TRUE(lcTest().isDebugEnabled());
	EXPECT_TRUE(lcOther().isInfoEnabled());
	EXPECT_FALSE(lcNetwork().isDebugEnabled());

	// Also over a level that would enable it
	qputenv("QT_LOGGING_RULES", "qredmine.test.info=false");
	manager->setLevel(LogManager::Debug);
	EXPECT_FALSE(lcTest().isInfoEnabled());
	EXPECT_TRUE(lcNetwork().isDebugEnabled());
}

TEST_F(LogTest, LoggingASuppressedLevelWritesNothing)
{
	createManager();
	QR_DEBUG(lcTest, QStringLiteral("hidden"));
	qCDebug(lcOther) << "hidden too";
	EXPECT_TRUE(readLog().isEmpty());
	EXPECT_TRUE(manager->bufferedEntries().isEmpty());
}

TEST_F(LogTest, LogStartupAndShutdownWriteEntries)
{
	createManager();
	manager->logStartup();
	manager->logShutdown();
	const QList<QJsonObject> entries = readLog();
	ASSERT_GE(entries.size(), 3);
	EXPECT_EQ(entries.first().value("cat").toString().toStdString(), "qredmine.app");
	EXPECT_TRUE(entries.first().value("fields").toObject().contains("qt"));
	EXPECT_EQ(entries.last().value("msg").toString().toStdString(), "Application is shutting down");
}

namespace {
void writeFile(const QString& fileName, const QByteArray& content)
{
	QFile file(fileName);
	ASSERT_TRUE(file.open(QIODevice::WriteOnly));
	file.write(content);
}
}

TEST_F(LogTest, ExportConcatenatesRotatedFilesOldestToNewest)
{
	writeFile(path(QStringLiteral("stride.2.log")), "{\"msg\":\"oldest\"}\n");
	writeFile(path(QStringLiteral("stride.1.log")), "{\"msg\":\"older\"}\n");
	createManager();
	QR_INFO(lcTest, QStringLiteral("newest"));

	const QString target = path(QStringLiteral("export.jsonl"));
	ASSERT_TRUE(manager->exportLogs(target));
	QFile file(target);
	ASSERT_TRUE(file.open(QIODevice::ReadOnly));
	const QList<QByteArray> lines = file.readAll().split('\n');
	ASSERT_EQ(lines.size(), 4); // three and the empty piece after the last newline
	EXPECT_EQ(lines.at(0).toStdString(), "{\"msg\":\"oldest\"}");
	EXPECT_EQ(lines.at(1).toStdString(), "{\"msg\":\"older\"}");
	EXPECT_TRUE(lines.at(2).contains("newest"));
	// The export isn't one of the log files
	EXPECT_EQ(manager->logFiles().size(), 3);
}

TEST_F(LogTest, ExportReplacesAnExistingTarget)
{
	createManager();
	QR_INFO(lcTest, QStringLiteral("entry"));
	const QString target = path(QStringLiteral("export.jsonl"));
	writeFile(target, "old content that is longer than the log\n");
	ASSERT_TRUE(manager->exportLogs(target));
	EXPECT_FALSE(readRaw(QStringLiteral("export.jsonl")).contains("old content"));
	EXPECT_TRUE(readRaw(QStringLiteral("export.jsonl")).contains("entry"));
}

TEST_F(LogTest, ExportRefusesToOverwriteALogFile)
{
	createManager();
	QR_INFO(lcTest, QStringLiteral("keep me"));
	const QByteArray before = readRaw();
	ASSERT_FALSE(before.isEmpty());

	EXPECT_FALSE(manager->exportLogs(path(QStringLiteral("stride.log"))));
	// The same file by another path
	EXPECT_FALSE(manager->exportLogs(directory.path() + QStringLiteral("/./stride.log")));
	EXPECT_EQ(readRaw(), before);
}

TEST_F(LogTest, ExportFailsForAnUnwritableTarget)
{
	createManager();
	QR_INFO(lcTest, QStringLiteral("entry"));
	EXPECT_FALSE(manager->exportLogs(path(QStringLiteral("missing-dir/export.jsonl"))));
}
