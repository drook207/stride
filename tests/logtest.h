#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QTemporaryDir>
#include <gtest/gtest.h>
#include <memory>

#include "logmanager.h"

/*!
 * \brief The LogTest class Fixture owning a LogManager that logs into a temporary directory. The console output
 * is swallowed by a no-op "previous" message handler, the persisted level and QT_LOGGING_RULES are reset before
 * and restored after each test, and the manager is destroyed in TearDown() so tests don't affect each other.
 */
class LogTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;

	//! (Re-)creates the manager in the temporary directory
	void createManager();
	void destroyManager();

	//! Flushes and returns the parsed lines of a log file (the current one by default), asserting each is valid JSON
	QList<QJsonObject> readLog(const QString& fileName = QStringLiteral("stride.log"));
	//! Flushes and returns the raw content of a log file
	QByteArray readRaw(const QString& fileName = QStringLiteral("stride.log"));
	QString path(const QString& fileName) const;

	//! The parsed entry of the log file with this message, an empty object if there is none
	QJsonObject findInLog(const QString& message);

	QTemporaryDir directory;
	std::unique_ptr<QRedmineClient::Utils::Logging::LogManager> manager;

private:
	QtMessageHandler mOriginalHandler = nullptr;
	QByteArray mOriginalRules;
	bool mHadRules = false;
};
