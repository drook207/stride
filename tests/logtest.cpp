#include "logtest.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSettings>

using QRedmineClient::Utils::Logging::LogManager;

namespace {
void swallowMessage(QtMsgType, const QMessageLogContext&, const QString&) { }
}

void LogTest::SetUp()
{
	ASSERT_TRUE(directory.isValid());
	QSettings().clear();
	mHadRules = qEnvironmentVariableIsSet("QT_LOGGING_RULES");
	mOriginalRules = qgetenv("QT_LOGGING_RULES");
	qunsetenv("QT_LOGGING_RULES");
	mOriginalHandler = qInstallMessageHandler(&swallowMessage);
}

void LogTest::TearDown()
{
	destroyManager();
	qInstallMessageHandler(mOriginalHandler);
	if (mHadRules) {
		qputenv("QT_LOGGING_RULES", mOriginalRules);
	} else {
		qunsetenv("QT_LOGGING_RULES");
	}
	// A manager leaves the filter rules it applied behind
	QLoggingCategory::setFilterRules(QString());
	QSettings().clear();
}

void LogTest::createManager() { manager = std::make_unique<LogManager>(directory.path()); }

void LogTest::destroyManager() { manager.reset(); }

QString LogTest::path(const QString& fileName) const { return QDir(directory.path()).filePath(fileName); }

QByteArray LogTest::readRaw(const QString& fileName)
{
	if (manager) {
		manager->flush();
	}
	QFile file(path(fileName));
	if (!file.open(QIODevice::ReadOnly)) {
		return {};
	}
	return file.readAll();
}

QList<QJsonObject> LogTest::readLog(const QString& fileName)
{
	QList<QJsonObject> entries;
	const QList<QByteArray> lines = readRaw(fileName).split('\n');
	for (qsizetype i = 0; i < lines.size(); ++i) {
		if (lines.at(i).isEmpty()) {
			// Only the (empty) piece after the final newline may be empty
			EXPECT_EQ(i, lines.size() - 1) << "empty line in the log";
			continue;
		}
		QJsonParseError error;
		const QJsonDocument document = QJsonDocument::fromJson(lines.at(i), &error);
		EXPECT_EQ(error.error, QJsonParseError::NoError) << lines.at(i).left(200).toStdString();
		EXPECT_TRUE(document.isObject());
		entries.append(document.object());
	}
	return entries;
}

QJsonObject LogTest::findInLog(const QString& message)
{
	for (const QJsonObject& entry : readLog()) {
		if (entry.value("msg").toString() == message) {
			return entry;
		}
	}
	return {};
}
