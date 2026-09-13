#pragma once

#include "mockhttpserver.h"
#include "redmineclient.h"
#include <gtest/gtest.h>
#include <memory>

/*!
 * \brief The RedmineClientIntegrationTest class Fixture wiring a RedmineClient up to a
 * MockHttpServer (a real loopback HTTP server) so requests exercise the actual Qt networking
 * stack end-to-end, instead of only unit-testing request construction. Settings are cleared
 * before and after each test for isolation (see RedmineClientTest for details); the client and
 * server are (re-)created fresh per test.
 */
class RedmineClientIntegrationTest : public ::testing::Test {
protected:
	static constexpr const char* API_KEY = "test-api-key";

	void SetUp() override;
	void TearDown() override;

	std::unique_ptr<QRedmineTesting::MockHttpServer> server;
	std::unique_ptr<QRedmineClient::RedmineClient> client;
};
