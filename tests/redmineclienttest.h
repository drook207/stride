#pragma once

#include "redmineclient.h"
#include <gtest/gtest.h>
#include <memory>

/*!
 * \brief The RedmineClientTest class Fixture for RedmineClient unit tests that do not require a
 * real/mock network round trip. Settings are cleared before and after each test, and the client
 * is (re-)constructed fresh in SetUp() after clearing settings, since RedmineClient reads its
 * baseUrl/apiKey from QSettings on construction (see qsettingshelper.h) and every test run must
 * start from a clean, isolated slate (see tests/main.cpp for the dedicated org/app name used to
 * keep this away from a developer's real settings).
 */
class RedmineClientTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;

	std::unique_ptr<QRedmineClient::RedmineClient> client;
};
