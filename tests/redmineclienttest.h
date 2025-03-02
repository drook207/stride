#pragma once

#include "redmineclient.h"
#include <gtest/gtest.h>

class RedmineClientTest : public ::testing::Test {
protected:
	QRedmineClient::RedmineClient client;
};
