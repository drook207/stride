#include "redmineclienttest.h"
#include <gtest/gtest.h>

TEST_F(RedmineClientTest, SetBaseUrl)
{
	const QString url = "http://localhost:8080/";
	client.setBaseUrl(url);

	EXPECT_EQ(client.baseUrl().toStdString(), url.toStdString());
}

TEST_F(RedmineClientTest, SetBaseUrlWithoutSlash)
{
	const QString url = "http://localhost:8080";
	client.setBaseUrl(url);

	EXPECT_NE(client.baseUrl().toStdString(), url.toStdString());
}
