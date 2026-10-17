#include "redmineclientintegrationtest.h"
#include "testutils.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSettings>
#include <QTcpServer>
#include <QTranslator>
#include <gtest/gtest.h>

using QRedmineClient::RedmineClient;
using QRedmineTesting::MockHttpResponse;
using QRedmineTesting::RecordedHttpRequest;
using QRedmineTesting::waitForSignal;
using QRedmineTesting::waitUntil;

void RedmineClientIntegrationTest::SetUp()
{
	QSettings settings;
	settings.clear();

	server = std::make_unique<QRedmineTesting::MockHttpServer>();
	client = std::make_unique<RedmineClient>();
	client->setApiKey(API_KEY);
	client->setBaseUrl(server->baseUrl());
}

void RedmineClientIntegrationTest::TearDown()
{
	client.reset();
	server.reset();
	QSettings settings;
	settings.clear();
}

namespace {
QJsonObject bodyAsObject(const RecordedHttpRequest& request) { return QJsonDocument::fromJson(request.body).object(); }
}

TEST_F(RedmineClientIntegrationTest, GetIssueSendsCorrectRequestAndEmitsIssueReceived)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issue":{"id":42,"subject":"Test issue"}})" });

	QJsonObject received;
	QObject::connect(client.get(), &RedmineClient::issueReceived, [&](const QJsonObject& issue) { received = issue; });

	client->getIssue(42);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issueReceived));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.method.toStdString(), "GET");
	EXPECT_EQ(request.path.toStdString(), "/issues/42.json");
	EXPECT_EQ(request.header("x-redmine-api-key").toStdString(), API_KEY);
	EXPECT_EQ(request.header("content-type").toStdString(), "application/json");

	EXPECT_EQ(received["id"].toInt(), 42);
	EXPECT_EQ(received["subject"].toString().toStdString(), "Test issue");

	EXPECT_FALSE(client->loading());
	EXPECT_EQ(client->connectionState(), RedmineClient::ONLINE);
}

TEST_F(RedmineClientIntegrationTest, GetIssuesWithFiltersSendsQueryParametersAndEmitsIssuesReceived)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issues":[{"id":1},{"id":2}]})" });

	QJsonArray received;
	QObject::connect(
		client.get(), &RedmineClient::issuesReceived, [&](const QJsonArray& issues) { received = issues; });

	client->getIssues(5, 2, 10, 20);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issuesReceived));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.method.toStdString(), "GET");
	EXPECT_EQ(request.path.toStdString(), "/issues.json");
	EXPECT_EQ(request.query.queryItemValue("project_id").toStdString(), "5");
	EXPECT_EQ(request.query.queryItemValue("status_id").toStdString(), "2");
	EXPECT_EQ(request.query.queryItemValue("limit").toStdString(), "10");
	EXPECT_EQ(request.query.queryItemValue("offset").toStdString(), "20");

	EXPECT_EQ(received.size(), 2);
}

TEST_F(RedmineClientIntegrationTest, CreateIssueSendsPostBodyAndEmitsIssueCreated)
{
	server->enqueueResponse(MockHttpResponse { 201, "Created", R"({"issue":{"id":7,"subject":"New"}})" });

	QJsonObject received;
	QObject::connect(client.get(), &RedmineClient::issueCreated, [&](const QJsonObject& issue) { received = issue; });

	client->createIssue(3, "New", "Desc", 2, 4);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issueCreated));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.method.toStdString(), "POST");
	EXPECT_EQ(request.path.toStdString(), "/issues.json");

	QJsonObject issue = bodyAsObject(request)["issue"].toObject();
	EXPECT_EQ(issue["project_id"].toInt(), 3);
	EXPECT_EQ(issue["subject"].toString().toStdString(), "New");
	EXPECT_EQ(issue["description"].toString().toStdString(), "Desc");
	EXPECT_EQ(issue["tracker_id"].toInt(), 2);
	EXPECT_EQ(issue["priority_id"].toInt(), 4);

	EXPECT_EQ(received["id"].toInt(), 7);
}

TEST_F(RedmineClientIntegrationTest, UpdateIssueSendsPutBodyAndEmitsIssueUpdated)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", "" });

	int updatedId = -1;
	QObject::connect(client.get(), &RedmineClient::issueUpdated, [&](int id) { updatedId = id; });

	client->updateIssue(15, "Updated subject");
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issueUpdated));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.method.toStdString(), "PUT");
	EXPECT_EQ(request.path.toStdString(), "/issues/15.json");

	QJsonObject issue = bodyAsObject(request)["issue"].toObject();
	EXPECT_EQ(issue["subject"].toString().toStdString(), "Updated subject");
	EXPECT_FALSE(issue.contains("description"));
	EXPECT_FALSE(issue.contains("status_id"));

	EXPECT_EQ(updatedId, 15);
}

TEST_F(RedmineClientIntegrationTest, DeleteIssueSendsDeleteAndEmitsIssueDeleted)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", "" });

	int deletedId = -1;
	QObject::connect(client.get(), &RedmineClient::issueDeleted, [&](int id) { deletedId = id; });

	client->deleteIssue(9);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issueDeleted));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.method.toStdString(), "DELETE");
	EXPECT_EQ(request.path.toStdString(), "/issues/9.json");

	EXPECT_EQ(deletedId, 9);
}

TEST_F(RedmineClientIntegrationTest, GetProjectSendsCorrectRequestAndEmitsProjectReceived)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"project":{"id":3,"name":"Proj"}})" });

	QJsonObject received;
	QObject::connect(
		client.get(), &RedmineClient::projectReceived, [&](const QJsonObject& project) { received = project; });

	client->getProject(3);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::projectReceived));

	ASSERT_EQ(server->requests().size(), 1);
	EXPECT_EQ(server->requests().first().path.toStdString(), "/projects/3.json");
	EXPECT_EQ(received["id"].toInt(), 3);
}

TEST_F(RedmineClientIntegrationTest, GetProjectsWithPaginationSendsQueryParameters)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"projects":[{"id":1}]})" });

	client->getProjects(25, 50);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::projectsReceived));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.path.toStdString(), "/projects.json");
	EXPECT_EQ(request.query.queryItemValue("limit").toStdString(), "25");
	EXPECT_EQ(request.query.queryItemValue("offset").toStdString(), "50");
	// The activities each project allows come along, see TimeEntryActivitiesModel.
	EXPECT_EQ(request.query.queryItemValue("include").toStdString(), "time_entry_activities,enabled_modules");
}

TEST_F(RedmineClientIntegrationTest, GetAllProjectsRequestsEveryPageAndEmitsThemOnce)
{
	server->enqueueResponse(
		MockHttpResponse { 200, "OK", R"({"projects":[{"id":1},{"id":2}],"total_count":3,"offset":0,"limit":100})" });
	server->enqueueResponse(
		MockHttpResponse { 200, "OK", R"({"projects":[{"id":3}],"total_count":3,"offset":2,"limit":100})" });

	int emitted = 0;
	QJsonArray received;
	QObject::connect(client.get(), &RedmineClient::projectsReceived, [&](const QJsonArray& projects) {
		++emitted;
		received = projects;
	});

	client->getAllProjects();
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::projectsReceived));

	ASSERT_EQ(server->requests().size(), 2);
	EXPECT_EQ(server->requests().at(0).query.queryItemValue("offset").toStdString(), "0");
	EXPECT_EQ(server->requests().at(1).query.queryItemValue("offset").toStdString(), "2");
	EXPECT_EQ(emitted, 1);
	ASSERT_EQ(received.size(), 3);
	EXPECT_EQ(received.at(2).toObject()["id"].toInt(), 3);
}

TEST_F(RedmineClientIntegrationTest, GetCurrentUserRequestsMembershipsAndEmitsCurrentUserReceived)
{
	server->enqueueResponse(MockHttpResponse {
		200, "OK", R"({"user":{"id":5,"login":"alice","memberships":[{"project":{"id":28},"roles":[{"id":4}]}]}})" });

	QJsonObject received;
	QObject::connect(
		client.get(), &RedmineClient::currentUserReceived, [&](const QJsonObject& user) { received = user; });

	client->getCurrentUser();
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::currentUserReceived));

	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.path.toStdString(), "/users/current.json");
	EXPECT_EQ(request.query.queryItemValue("include").toStdString(), "memberships");
	EXPECT_EQ(received["login"].toString().toStdString(), "alice");
}

TEST_F(RedmineClientIntegrationTest, GetRoleEmitsRoleReceivedAndFailsQuietly)
{
	server->enqueueResponse(
		MockHttpResponse { 200, "OK", R"({"role":{"id":1,"name":"Non member","permissions":["add_issues"]}})" });
	server->enqueueResponse(MockHttpResponse { 403, "Forbidden", "" });

	QJsonObject received;
	bool errorFired = false;
	QObject::connect(client.get(), &RedmineClient::roleReceived, [&](const QJsonObject& role) { received = role; });
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { errorFired = true; });

	client->getRole(1);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::roleReceived));
	EXPECT_EQ(server->requests().first().path.toStdString(), "/roles/1.json");
	EXPECT_EQ(received["permissions"].toArray().first().toString().toStdString(), "add_issues");

	client->getRole(2);
	ASSERT_TRUE(waitUntil([&]() { return server->requests().size() == 2; }));
	waitUntil([]() { return false; }, 300); // time for the reply
	EXPECT_FALSE(errorFired);
}

TEST_F(RedmineClientIntegrationTest, GetOwnTimeEntriesRequestsEveryPageWithoutEmittingTimeEntriesReceived)
{
	server->enqueueResponse(MockHttpResponse {
		200, "OK", R"({"time_entries":[{"id":1},{"id":2}],"total_count":3,"offset":0,"limit":100})" });
	server->enqueueResponse(
		MockHttpResponse { 200, "OK", R"({"time_entries":[{"id":3}],"total_count":3,"offset":2,"limit":100})" });

	QJsonArray received;
	bool searchResultsFired = false;
	QObject::connect(client.get(), &RedmineClient::ownTimeEntriesReceived,
		[&](const QJsonArray& timeEntries) { received = timeEntries; });
	QObject::connect(
		client.get(), &RedmineClient::timeEntriesReceived, [&](const QJsonArray&) { searchResultsFired = true; });

	client->getOwnTimeEntries("2026-10-01", "2026-10-17");
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::ownTimeEntriesReceived));

	ASSERT_EQ(server->requests().size(), 2);
	const RecordedHttpRequest& first = server->requests().first();
	EXPECT_EQ(first.path.toStdString(), "/time_entries.json");
	EXPECT_EQ(first.query.queryItemValue("user_id").toStdString(), "me");
	EXPECT_EQ(first.query.queryItemValue("from").toStdString(), "2026-10-01");
	EXPECT_EQ(first.query.queryItemValue("to").toStdString(), "2026-10-17");
	EXPECT_EQ(server->requests().at(1).query.queryItemValue("offset").toStdString(), "2");
	EXPECT_EQ(received.size(), 3);
	EXPECT_FALSE(searchResultsFired);
}

TEST_F(RedmineClientIntegrationTest, CreateProjectSendsPostBodyAndEmitsProjectCreated)
{
	server->enqueueResponse(MockHttpResponse { 201, "Created", R"({"project":{"id":8,"identifier":"proj"}})" });

	QJsonObject received;
	QObject::connect(
		client.get(), &RedmineClient::projectCreated, [&](const QJsonObject& project) { received = project; });

	client->createProject("Proj Name", "proj", "Desc");
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::projectCreated));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.method.toStdString(), "POST");
	EXPECT_EQ(request.path.toStdString(), "/projects.json");

	QJsonObject project = bodyAsObject(request)["project"].toObject();
	EXPECT_EQ(project["name"].toString().toStdString(), "Proj Name");
	EXPECT_EQ(project["identifier"].toString().toStdString(), "proj");
	EXPECT_EQ(project["description"].toString().toStdString(), "Desc");

	EXPECT_EQ(received["id"].toInt(), 8);
}

TEST_F(RedmineClientIntegrationTest, UpdateProjectSendsPutBodyAndEmitsProjectUpdated)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", "" });

	int updatedId = -1;
	QObject::connect(client.get(), &RedmineClient::projectUpdated, [&](int id) { updatedId = id; });

	client->updateProject(4, "New name");
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::projectUpdated));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.method.toStdString(), "PUT");
	EXPECT_EQ(request.path.toStdString(), "/projects/4.json");

	QJsonObject project = bodyAsObject(request)["project"].toObject();
	EXPECT_EQ(project["name"].toString().toStdString(), "New name");
	EXPECT_FALSE(project.contains("description"));

	EXPECT_EQ(updatedId, 4);
}

TEST_F(RedmineClientIntegrationTest, DeleteProjectSendsDeleteAndEmitsProjectDeleted)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", "" });

	int deletedId = -1;
	QObject::connect(client.get(), &RedmineClient::projectDeleted, [&](int id) { deletedId = id; });

	client->deleteProject(11);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::projectDeleted));

	ASSERT_EQ(server->requests().size(), 1);
	EXPECT_EQ(server->requests().first().path.toStdString(), "/projects/11.json");
	EXPECT_EQ(deletedId, 11);
}

TEST_F(RedmineClientIntegrationTest, CreateTimeEntrySendsPostBodyAndEmitsTimeEntryCreated)
{
	server->enqueueResponse(MockHttpResponse { 201, "Created", R"({"time_entry":{"id":55,"hours":2.5}})" });

	QJsonObject received;
	int receivedTag = -1;
	QObject::connect(client.get(), &RedmineClient::timeEntryCreated, [&](const QJsonObject& timeEntry, int tag) {
		received = timeEntry;
		receivedTag = tag;
	});

	client->createTimeEntry(42, 2.5, "Worked", "2024-01-01", 4, {}, 17);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryCreated));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.method.toStdString(), "POST");
	EXPECT_EQ(request.path.toStdString(), "/time_entries.json");

	QJsonObject timeEntry = bodyAsObject(request)["time_entry"].toObject();
	EXPECT_EQ(timeEntry["issue_id"].toInt(), 42);
	EXPECT_DOUBLE_EQ(timeEntry["hours"].toDouble(), 2.5);
	EXPECT_EQ(timeEntry["comments"].toString().toStdString(), "Worked");
	EXPECT_EQ(timeEntry["spent_on"].toString().toStdString(), "2024-01-01");

	EXPECT_EQ(received["id"].toInt(), 55);
	EXPECT_EQ(receivedTag, 17);
}

TEST_F(RedmineClientIntegrationTest, GetTimeEntriesSendsIssueIdQueryParameterAndEmitsTimeEntriesReceived)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"time_entries":[{"id":1}]})" });

	client->getTimeEntries(42);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntriesReceived));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.path.toStdString(), "/time_entries.json");
	EXPECT_EQ(request.query.queryItemValue("issue_id").toStdString(), "42");
}

TEST_F(RedmineClientIntegrationTest, DeleteTimeEntrySendsDeleteAndEmitsTimeEntryDeleted)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", "" });

	int deletedId = -1;
	QObject::connect(client.get(), &RedmineClient::timeEntryDeleted, [&](int id) { deletedId = id; });

	client->deleteTimeEntry(77);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryDeleted));

	ASSERT_EQ(server->requests().size(), 1);
	EXPECT_EQ(server->requests().first().path.toStdString(), "/time_entries/77.json");
	EXPECT_EQ(deletedId, 77);
}

TEST_F(RedmineClientIntegrationTest, UpdateTimeEntrySendsChangedFieldsAndEmitsTimeEntryUpdated)
{
	server->enqueueResponse(MockHttpResponse { 204, "No Content", "" });

	int updatedId = -1;
	int updatedTag = -1;
	QObject::connect(client.get(), &RedmineClient::timeEntryUpdated, [&](int id, int tag) {
		updatedId = id;
		updatedTag = tag;
	});

	client->updateTimeEntry(9, 2.5, "Reworked", "2026-10-15", 8, {}, 3);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryUpdated));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.method.toStdString(), "PUT");
	EXPECT_EQ(request.path.toStdString(), "/time_entries/9.json");

	QJsonObject timeEntry = bodyAsObject(request)["time_entry"].toObject();
	EXPECT_DOUBLE_EQ(timeEntry["hours"].toDouble(), 2.5);
	EXPECT_EQ(timeEntry["comments"].toString().toStdString(), "Reworked");
	EXPECT_EQ(timeEntry["spent_on"].toString().toStdString(), "2026-10-15");
	EXPECT_EQ(timeEntry["activity_id"].toInt(), 8);

	EXPECT_EQ(updatedId, 9);
	EXPECT_EQ(updatedTag, 3);
}

TEST_F(RedmineClientIntegrationTest, RejectedUpdateTimeEntryEmitsTimeEntryUpdateFailed)
{
	server->enqueueResponse(
		MockHttpResponse { 422, "Unprocessable Entity", R"({"errors":["Activity is not included in the list"]})" });

	int failedId = -1;
	int failedTag = -1;
	bool updated = false;
	QObject::connect(client.get(), &RedmineClient::timeEntryUpdateFailed, [&](int id, int tag) {
		failedId = id;
		failedTag = tag;
	});
	QObject::connect(client.get(), &RedmineClient::timeEntryUpdated, [&]() { updated = true; });

	client->updateTimeEntry(9, 2.5, "Reworked", "2026-10-15", 99, {}, 5);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryUpdateFailed));

	EXPECT_EQ(failedId, 9);
	EXPECT_EQ(failedTag, 5);
	EXPECT_FALSE(updated);
}

TEST_F(RedmineClientIntegrationTest, UpdateTimeEntryLeavesDateAndActivityUnchangedWhenNotGiven)
{
	server->enqueueResponse(MockHttpResponse { 204, "No Content", "" });

	client->updateTimeEntry(9, 1.0, "Only hours and comment");
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryUpdated));

	QJsonObject timeEntry = bodyAsObject(server->requests().first())["time_entry"].toObject();
	EXPECT_FALSE(timeEntry.contains("spent_on"));
	EXPECT_FALSE(timeEntry.contains("activity_id"));
}

TEST_F(RedmineClientIntegrationTest, UpdatedIdIsParsedWhenBaseUrlHasASubpath)
{
	// Redmine installed under a subpath, e.g. https://host/redmine/
	client->setBaseUrl(server->baseUrl() + QStringLiteral("redmine/"));
	server->enqueueResponse(MockHttpResponse { 204, "No Content", "" });

	int updatedId = -1;
	QObject::connect(client.get(), &RedmineClient::timeEntryUpdated, [&](int id) { updatedId = id; });

	client->updateTimeEntry(31, 1.0, "");
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryUpdated));

	EXPECT_EQ(server->requests().first().path.toStdString(), "/redmine/time_entries/31.json");
	EXPECT_EQ(updatedId, 31);
}

TEST_F(RedmineClientIntegrationTest, TimeEntryCustomFieldsAreTakenFromCustomFieldsListForAdmins)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK",
		R"({"custom_fields":[
			{"id":1,"name":"Special","customized_type":"time_entry","field_format":"list",
			 "possible_values":[{"value":"Overtime","label":"Overtime"},{"value":"Weekend","label":"Weekend"}]},
			{"id":3,"name":"Customer","customized_type":"issue","field_format":"list","possible_values":[]}]})" });

	QJsonArray fields;
	QObject::connect(client.get(), &RedmineClient::timeEntryCustomFieldsReceived,
		[&](const QJsonArray& received) { fields = received; });

	client->getTimeEntryCustomFields();
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryCustomFieldsReceived));

	EXPECT_EQ(server->requests().first().path.toStdString(), "/custom_fields.json");
	ASSERT_EQ(fields.size(), 1); // the issue field is left out
	EXPECT_EQ(fields[0].toObject()["name"].toString().toStdString(), "Special");
	EXPECT_EQ(fields[0].toObject()["possible_values"].toArray().size(), 2);
}

TEST_F(RedmineClientIntegrationTest, TimeEntryCustomFieldsFallBackToTimeEntriesWithoutReportingAnError)
{
	// Non-admins may not list custom fields; the fields are then derived from recent time entries.
	server->enqueueResponse(MockHttpResponse { 403, "Forbidden", "" });
	server->enqueueResponse(MockHttpResponse { 200, "OK",
		R"({"time_entries":[
			{"id":1,"custom_fields":[{"id":1,"name":"Special","value":"Overtime"},{"id":2,"name":"Billable","value":"1"}]},
			{"id":2,"custom_fields":[{"id":1,"name":"Special","value":null},{"id":2,"name":"Billable","value":"0"}]},
			{"id":3,"custom_fields":[{"id":1,"name":"Special","value":"On-call"},{"id":2,"name":"Billable","value":"1"}]}]})" });

	bool errorFired = false;
	bool entriesFired = false;
	QJsonArray fields;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { errorFired = true; });
	QObject::connect(
		client.get(), &RedmineClient::timeEntriesReceived, [&](const QJsonArray&) { entriesFired = true; });
	QObject::connect(client.get(), &RedmineClient::timeEntryCustomFieldsReceived,
		[&](const QJsonArray& received) { fields = received; });

	client->getTimeEntryCustomFields();
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryCustomFieldsReceived));

	ASSERT_EQ(server->requests().size(), 2);
	EXPECT_EQ(server->requests()[1].path.toStdString(), "/time_entries.json");
	EXPECT_FALSE(errorFired);
	EXPECT_FALSE(entriesFired); // not mistaken for e.g. Search Issue results

	ASSERT_EQ(fields.size(), 2);
	const QJsonObject special = fields[0].toObject();
	EXPECT_EQ(special["name"].toString().toStdString(), "Special");
	EXPECT_EQ(special["field_format"].toString().toStdString(), "list");
	const QJsonArray values = special["possible_values"].toArray();
	ASSERT_EQ(values.size(), 2);
	EXPECT_EQ(values[0].toObject()["value"].toString().toStdString(), "On-call");
	EXPECT_EQ(values[1].toObject()["value"].toString().toStdString(), "Overtime");
	EXPECT_EQ(fields[1].toObject()["field_format"].toString().toStdString(), "bool");
}

TEST_F(RedmineClientIntegrationTest, TimeEntryCustomFieldsDontFallBackOnServerErrors)
{
	// Only "not allowed to list them" (403) is worth asking the time entries; other failures would
	// fail there too.
	server->enqueueResponse(MockHttpResponse { 500, "Internal Server Error", "" });

	bool fieldsFired = false;
	QObject::connect(
		client.get(), &RedmineClient::timeEntryCustomFieldsReceived, [&](const QJsonArray&) { fieldsFired = true; });

	client->getTimeEntryCustomFields();
	ASSERT_TRUE(waitUntil([&]() { return server->requests().size() == 1; }));
	waitUntil([]() { return false; }, 300); // time for the reply and a fallback request

	EXPECT_EQ(server->requests().size(), 1);
	EXPECT_FALSE(fieldsFired);
}

TEST_F(RedmineClientIntegrationTest, UpdateTimeEntryRequestTagIsNotMistakenForCustomFieldsRouting)
{
	// Request tags share no attribute with the routing of getTimeEntryCustomFields().
	server->enqueueResponse(MockHttpResponse { 204, "No Content", "" });

	int updatedTag = -1;
	QObject::connect(client.get(), &RedmineClient::timeEntryUpdated, [&](int, int tag) { updatedTag = tag; });

	client->updateTimeEntry(9, 1.0, "", "", 0, {}, 1);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryUpdated));

	EXPECT_EQ(updatedTag, 1);
}

TEST_F(RedmineClientIntegrationTest, CreateAndUpdateTimeEntrySendCustomFields)
{
	const QVariantList customFields { QVariantMap { { "id", 1 }, { "value", "Overtime" } } };

	server->enqueueResponse(MockHttpResponse { 201, "Created", R"({"time_entry":{"id":5}})" });
	client->createTimeEntry(42, 1.0, "", "2026-10-16", 9, customFields);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryCreated));

	server->enqueueResponse(MockHttpResponse { 204, "No Content", "" });
	client->updateTimeEntry(5, 2.0, "", "", 0, customFields);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryUpdated));

	ASSERT_EQ(server->requests().size(), 2);
	for (const auto& request : server->requests()) {
		const QJsonArray sent = bodyAsObject(request)["time_entry"].toObject()["custom_fields"].toArray();
		ASSERT_EQ(sent.size(), 1);
		EXPECT_EQ(sent[0].toObject()["id"].toInt(), 1);
		EXPECT_EQ(sent[0].toObject()["value"].toString().toStdString(), "Overtime");
	}
}

TEST_F(RedmineClientIntegrationTest, GetEnumerationsTimeEntryActivitiesEmitsTimeEntryActivitiesReceived)
{
	server->enqueueResponse(
		MockHttpResponse { 200, "OK", R"({"time_entry_activities":[{"id":4,"name":"Development"}]})" });

	client->getEnumerations(RedmineClient::TIME_ENTRY_ACTIVITIES);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryActivitiesReceived));

	ASSERT_EQ(server->requests().size(), 1);
	EXPECT_EQ(server->requests().first().path.toStdString(), "/enumerations/time_entry_activities.json");
}

TEST_F(RedmineClientIntegrationTest, DetectApiVersionReadsHeaderAndEmitsApiVersionChanged)
{
	MockHttpResponse response { 200, "OK", R"({"issues":[]})" };
	response.headers.insert("X-Redmine-API-Version", "5");
	server->enqueueResponse(response);

	client->detectApiVersion();
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::apiVersionChanged));

	EXPECT_EQ(client->apiVersion(), 5);
	EXPECT_TRUE(client->hasApiVersion(5));
	EXPECT_FALSE(client->hasApiVersion(6));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.path.toStdString(), "/issues.json");
	EXPECT_EQ(request.query.queryItemValue("limit").toStdString(), "0");
}

TEST_F(RedmineClientIntegrationTest, NotFoundResponseDoesNotEmitErrorOccurred)
{
	server->enqueueResponse(MockHttpResponse { 404, "Not Found", "" });

	bool errorFired = false;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { errorFired = true; });

	client->getIssue(999);
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));

	EXPECT_FALSE(errorFired);
	EXPECT_EQ(server->requests().size(), 1);
}

TEST_F(RedmineClientIntegrationTest, QuietGetIssueDoesNotEmitErrorOccurred)
{
	server->enqueueResponse(MockHttpResponse { 403, "Forbidden", "" });

	bool errorFired = false;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { errorFired = true; });

	client->getIssue(7, true);
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));

	EXPECT_FALSE(errorFired);
	EXPECT_EQ(server->requests().size(), 1);
}

TEST_F(RedmineClientIntegrationTest, ServerErrorResponseEmitsErrorOccurred)
{
	server->enqueueResponse(MockHttpResponse { 500, "Internal Server Error", R"({"errors":["boom"]})" });

	QString errorMessage;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString& error) { errorMessage = error; });

	client->getIssue(1);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::errorOccurred));

	EXPECT_FALSE(errorMessage.isEmpty());
	EXPECT_FALSE(client->loading());
}

TEST_F(RedmineClientIntegrationTest, ValidationErrorResponsePassesRedmineErrorsThrough)
{
	server->enqueueResponse(MockHttpResponse {
		422, "Unprocessable Entity", R"({"errors":["Subject cannot be blank","Tracker is invalid"]})" });

	QString errorMessage;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString& error) { errorMessage = error; });

	client->createIssue(1, QString());
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::errorOccurred));

	EXPECT_EQ(errorMessage.toStdString(),
		"The Redmine server rejected the request: Subject cannot be blank, Tracker is invalid");
}

TEST_F(RedmineClientIntegrationTest, ErrorMessageIsTranslatedWithEmbeddedGermanCatalog)
{
	// Loads the catalog the same way RedmineClient::installTranslations() does, but keeps the
	// translator local so it can be removed again and doesn't leak into other tests.
	QTranslator translator;
	ASSERT_TRUE(translator.load(
		QLocale(QLocale::German), QStringLiteral("QRedmineClient"), QStringLiteral("_"), QStringLiteral(":/i18n")));
	QCoreApplication::installTranslator(&translator);

	server->enqueueResponse(MockHttpResponse { 401, "Unauthorized", "" });

	QString errorMessage;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString& error) { errorMessage = error; });

	client->getIssue(1);
	const bool errorEmitted = waitForSignal(client.get(), &RedmineClient::errorOccurred);
	QCoreApplication::removeTranslator(&translator);
	ASSERT_TRUE(errorEmitted);

	EXPECT_EQ(errorMessage,
		QStringLiteral("Anmeldung fehlgeschlagen. Bitte prüfen Sie Ihren API-Schlüssel in den Einstellungen."));
}

TEST_F(RedmineClientIntegrationTest, ConnectionStateBecomesTemporaryOfflineAfterAuthErrorOnceOnline)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issue":{"id":1}})" });
	client->getIssue(1);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issueReceived));
	ASSERT_EQ(client->connectionState(), RedmineClient::ONLINE);

	server->enqueueResponse(MockHttpResponse { 401, "Unauthorized", "" });
	client->getIssue(2);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::errorOccurred));

	EXPECT_EQ(client->connectionState(), RedmineClient::TEMPORARY_OFFLINE);
}

TEST_F(RedmineClientIntegrationTest, ConnectionRefusedToClosedPortTriggersErrorOccurredAndStaysOffline)
{
	// Grab an ephemeral port and immediately release it again, so we get a port that (very likely)
	// has nothing listening on it, without hardcoding a port number that might already be in use.
	QTcpServer probe;
	probe.listen(QHostAddress::LocalHost, 0);
	quint16 closedPort = probe.serverPort();
	probe.close();

	client->setBaseUrl(QStringLiteral("http://127.0.0.1:%0/").arg(closedPort));

	bool errorFired = false;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { errorFired = true; });

	ASSERT_EQ(client->connectionState(), RedmineClient::OFFLINE);
	client->getIssue(1);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::errorOccurred));

	EXPECT_TRUE(errorFired);
	EXPECT_EQ(client->connectionState(), RedmineClient::OFFLINE);
}

TEST_F(RedmineClientIntegrationTest, LoadingIsTrueDuringRequestAndFalseAfterResponse)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issue":{"id":1}})" });

	ASSERT_FALSE(client->loading());
	client->getIssue(1);
	EXPECT_TRUE(client->loading());

	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issueReceived));
	EXPECT_FALSE(client->loading());
}
