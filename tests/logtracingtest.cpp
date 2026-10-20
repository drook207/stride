#include "logtest.h"
#include "mockhttpserver.h"
#include "redmineclient.h"
#include "testutils.h"

#include <QJsonDocument>
#include <QSettings>
#include <gtest/gtest.h>

using QRedmineClient::RedmineClient;
using QRedmineClient::Utils::Logging::LogEntry;
using QRedmineClient::Utils::Logging::LogManager;
using QRedmineTesting::MockHttpResponse;
using QRedmineTesting::waitUntil;

/*!
 * \brief The RequestTracingTest class A RedmineClient talking to a MockHttpServer while a LogManager records at
 * the verbose level
 */
class RequestTracingTest : public LogTest {
protected:
	static constexpr const char* API_KEY = "secret-key-for-the-trace-test";

	void SetUp() override
	{
		LogTest::SetUp();
		QSettings().setValue(LogManager::LEVEL_SETTING, QStringLiteral("verbose"));
		createManager();
		server = std::make_unique<QRedmineTesting::MockHttpServer>();
		client = std::make_unique<RedmineClient>();
		client->setApiKey(API_KEY);
		client->setBaseUrl(server->baseUrl());
	}

	void TearDown() override
	{
		client.reset();
		server.reset();
		LogTest::TearDown();
	}

	//! The entry of the network category that traces a finished request to \a path
	std::optional<LogEntry> traceOf(const QString& path) const
	{
		for (const LogEntry& entry : manager->bufferedEntries()) {
			if (entry.category == QLatin1String("qredmine.client.network") &&
				entry.fields.value("path").toString() == path && entry.fields.contains("status")) {
				return entry;
			}
		}
		return std::nullopt;
	}

	std::unique_ptr<QRedmineTesting::MockHttpServer> server;
	std::unique_ptr<RedmineClient> client;
};

TEST_F(RequestTracingTest, AFinishedRequestIsTracedWithItsDetails)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issue":{"id":42,"subject":"Test"}})" });
	client->getIssue(42);
	ASSERT_TRUE(waitUntil([&] { return traceOf(QStringLiteral("/issues/42.json")).has_value(); }));

	const LogEntry entry = *traceOf(QStringLiteral("/issues/42.json"));
	EXPECT_EQ(entry.type, QtInfoMsg);
	EXPECT_EQ(entry.fields.value("method").toString().toStdString(), "GET");
	EXPECT_EQ(entry.fields.value("status").toInt(), 200);
	EXPECT_GE(entry.fields.value("durationMs").toInt(-1), 0);
	EXPECT_GT(entry.fields.value("id").toInt(), 0);
	EXPECT_GT(entry.fields.value("bytes").toInt(), 0);
	EXPECT_FALSE(entry.fields.contains("error"));
	EXPECT_TRUE(entry.message.contains(QStringLiteral("/issues/42.json")));
}

TEST_F(RequestTracingTest, TheTraceIsInTheLogFileToo)
{
	client->getIssue(7);
	ASSERT_TRUE(waitUntil([&] { return traceOf(QStringLiteral("/issues/7.json")).has_value(); }));
	bool found = false;
	for (const QJsonObject& entry : readLog()) {
		found |= entry.value("cat").toString() == QLatin1String("qredmine.client.network") &&
			entry.value("fields").toObject().value("path").toString() == QLatin1String("/issues/7.json");
	}
	EXPECT_TRUE(found);
}

TEST_F(RequestTracingTest, AFailedRequestIsTracedAsAWarningWithItsError)
{
	server->enqueueResponse(MockHttpResponse { 500, "Internal Server Error", "{}" });
	client->getIssue(43);
	ASSERT_TRUE(waitUntil([&] { return traceOf(QStringLiteral("/issues/43.json")).has_value(); }));

	const LogEntry entry = *traceOf(QStringLiteral("/issues/43.json"));
	EXPECT_EQ(entry.type, QtWarningMsg);
	EXPECT_EQ(entry.fields.value("status").toInt(), 500);
	EXPECT_TRUE(entry.fields.contains("error"));
}

TEST_F(RequestTracingTest, NoEntryAndNoFileContainsTheApiKey)
{
	client->getIssue(42);
	client->getIssue(43);
	ASSERT_TRUE(waitUntil([&] {
		return traceOf(QStringLiteral("/issues/42.json")).has_value() &&
			traceOf(QStringLiteral("/issues/43.json")).has_value();
	}));
	// The debug entries of the sent requests are there, at the verbose level
	manager->flush();
	const QString key = QString::fromLatin1(API_KEY);
	ASSERT_FALSE(manager->bufferedEntries().isEmpty());
	for (const LogEntry& entry : manager->bufferedEntries()) {
		EXPECT_FALSE(entry.message.contains(key)) << entry.message.toStdString();
		EXPECT_FALSE(QJsonDocument(entry.fields).toJson().contains(key.toUtf8())) << entry.message.toStdString();
	}
	for (const QString& file : manager->logFiles()) {
		EXPECT_FALSE(readRaw(QFileInfo(file).fileName()).contains(API_KEY));
	}
}

TEST_F(RequestTracingTest, AnErrorBodyIsNotLoggedAtTheVerboseLevel)
{
	static constexpr const char* SECRET = "comment-the-user-typed";
	server->enqueueResponse(MockHttpResponse {
		422, "Unprocessable Entity", R"({"errors":["Comments is invalid: comment-the-user-typed"]})" });
	client->getIssue(44);
	ASSERT_TRUE(waitUntil([&] { return traceOf(QStringLiteral("/issues/44.json")).has_value(); }));
	manager->flush();

	for (const LogEntry& entry : manager->bufferedEntries()) {
		EXPECT_FALSE(QJsonDocument(entry.fields).toJson().contains(SECRET)) << entry.message.toStdString();
	}
	EXPECT_FALSE(readRaw().contains(SECRET));
}

TEST_F(RequestTracingTest, AnErrorBodyIsLoggedAtTheDebugLevel)
{
	static constexpr const char* SECRET = "comment-the-user-typed";
	manager->setLevel(LogManager::Debug);
	server->enqueueResponse(MockHttpResponse {
		422, "Unprocessable Entity", R"({"errors":["Comments is invalid: comment-the-user-typed"]})" });
	client->getIssue(45);
	ASSERT_TRUE(waitUntil([&] {
		for (const LogEntry& entry : manager->bufferedEntries()) {
			if (entry.category == QLatin1String("qredmine.content") &&
				entry.fields.value("body").toString().contains(SECRET)) {
				return true;
			}
		}
		return false;
	}));
	EXPECT_TRUE(readRaw().contains(SECRET));
}
