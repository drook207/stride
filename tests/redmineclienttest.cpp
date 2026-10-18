#include "redmineclienttest.h"
#include <QSettings>
#include <gtest/gtest.h>

using QRedmineClient::RedmineClient;

void RedmineClientTest::SetUp()
{
	QSettings settings;
	settings.clear();
	client = std::make_unique<RedmineClient>();
}

void RedmineClientTest::TearDown()
{
	client.reset();
	QSettings settings;
	settings.clear();
}

TEST_F(RedmineClientTest, SetBaseUrl)
{
	const QString url = "http://localhost:8080/";
	client->setBaseUrl(url);

	EXPECT_EQ(client->baseUrl().toStdString(), url.toStdString());
}

TEST_F(RedmineClientTest, SetBaseUrlWithoutSlash)
{
	const QString url = "http://localhost:8080";
	client->setBaseUrl(url);

	EXPECT_NE(client->baseUrl().toStdString(), url.toStdString());
	EXPECT_EQ(client->baseUrl().toStdString(), (url + "/").toStdString());
}

TEST_F(RedmineClientTest, SetBaseUrlEmitsChangeSignalOnlyWhenValueChanges)
{
	int changeCount = 0;
	QObject::connect(client.get(), &RedmineClient::baseUrlChanged, [&]() { ++changeCount; });

	client->setBaseUrl("http://localhost:8080/");
	EXPECT_EQ(changeCount, 1);

	client->setBaseUrl("http://localhost:8080/");
	EXPECT_EQ(changeCount, 1);

	client->setBaseUrl("http://localhost:9090/");
	EXPECT_EQ(changeCount, 2);
}

TEST_F(RedmineClientTest, SetApiKeyUpdatesValue)
{
	client->setApiKey("my-secret-key");

	EXPECT_EQ(client->apiKey().toStdString(), std::string("my-secret-key"));
}

TEST_F(RedmineClientTest, SetApiKeyEmitsChangeSignalOnlyWhenValueChanges)
{
	int changeCount = 0;
	QObject::connect(client.get(), &RedmineClient::apiKeyChanged, [&]() { ++changeCount; });

	client->setApiKey("key-a");
	EXPECT_EQ(changeCount, 1);

	client->setApiKey("key-a");
	EXPECT_EQ(changeCount, 1);

	client->setApiKey("key-b");
	EXPECT_EQ(changeCount, 2);
}

TEST_F(RedmineClientTest, SetApiKeyDoesNotPersistPlainTextInQSettings)
{
	// Regression test for the API key having been stored in plain text via QSettings: it must now
	// go exclusively through the OS-native secure storage (see securestorage.h), never QSettings.
	client->setApiKey("some-secret");

	QSettings settings;
	EXPECT_FALSE(settings.contains("client/apiKey"));
}

TEST_F(RedmineClientTest, ConstructorMigratesLegacyPlainTextApiKeyOutOfQSettings)
{
	// Simulate a pre-existing installation that still has the API key stored in plain text from
	// before the migration to secure storage existed.
	client.reset();
	{
		QSettings settings;
		settings.setValue("client/apiKey", "legacy-secret");
	}

	client = std::make_unique<RedmineClient>();

	EXPECT_EQ(client->apiKey().toStdString(), std::string("legacy-secret"));

	QSettings settings;
	EXPECT_FALSE(settings.contains("client/apiKey"));
}

TEST_F(RedmineClientTest, UseSSLDefaultsToTrue) { EXPECT_TRUE(client->useSSL()); }

TEST_F(RedmineClientTest, UseSSLTogglesAndEmitsChangeSignal)
{
	int changeCount = 0;
	QObject::connect(client.get(), &RedmineClient::useSSLChanged, [&]() { ++changeCount; });

	client->useSSL(true); // same as default, no change
	EXPECT_EQ(changeCount, 0);
	EXPECT_TRUE(client->useSSL());

	client->useSSL(false);
	EXPECT_EQ(changeCount, 1);
	EXPECT_FALSE(client->useSSL());

	client->useSSL(false); // no change
	EXPECT_EQ(changeCount, 1);
}

TEST_F(RedmineClientTest, DefaultConnectionStateIsOffline)
{
	EXPECT_EQ(client->connectionState(), RedmineClient::OFFLINE);
}

TEST_F(RedmineClientTest, DefaultLoadingIsFalse) { EXPECT_FALSE(client->loading()); }

TEST_F(RedmineClientTest, DefaultApiVersionIsUnknown)
{
	// By value: EXPECT_EQ takes a reference, and MinGW then imports the constant from the DLL,
	// which never exports it.
	EXPECT_EQ(client->apiVersion(), int { RedmineClient::UNKNOWN_API_VERSION });
}

TEST_F(RedmineClientTest, HasApiVersionIsFalseWhileVersionUnknown)
{
	EXPECT_FALSE(client->hasApiVersion(0));
	EXPECT_FALSE(client->hasApiVersion(1));
}

TEST_F(RedmineClientTest, RequestSlotSetsLoadingSynchronouslyBeforeAnyReply)
{
	// No event loop is pumped here: loading must flip to true inside the slot itself, synchronously,
	// well before a QNetworkAccessManager reply could possibly arrive.
	ASSERT_FALSE(client->loading());

	client->getIssue(1);

	EXPECT_TRUE(client->loading());
}
