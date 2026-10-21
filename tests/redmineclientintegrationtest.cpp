#include "redmineclientintegrationtest.h"
#include "testutils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
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

//! A port connections are refused on for as long as the object lives: it is bound, so no one else gets it,
//! but not listening.
class RefusedPort {
public:
	RefusedPort() { EXPECT_TRUE(mSocket.bind(QHostAddress::LocalHost, 0)); }

	QString baseUrl() const { return QStringLiteral("http://127.0.0.1:%0/").arg(mSocket.localPort()); }

private:
	QTcpSocket mSocket;
};
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

TEST_F(RedmineClientIntegrationTest, ReportsForDifferentPurposesDontSupersedeEachOther)
{
	server->enqueueResponse(
		MockHttpResponse { 200, "OK", R"({"time_entries":[{"id":1}],"total_count":1,"offset":0})" });
	server->enqueueResponse(
		MockHttpResponse { 200, "OK", R"({"time_entries":[{"id":2}],"total_count":1,"offset":0})" });

	QList<int> purposes;
	QObject::connect(client.get(), &RedmineClient::timeEntryReportReceived,
		[&](const QJsonArray&, const QString&, const QString&, int purpose) { purposes.append(purpose); });

	client->getTimeEntryReport("2026-10-12", "2026-10-18", RedmineClient::OverviewReport);
	client->getTimeEntryReport("2026-01-01", "2026-12-31", RedmineClient::ExportReport);
	ASSERT_TRUE(waitUntil([&]() { return purposes.size() == 2; }));
	EXPECT_TRUE(purposes.contains(RedmineClient::OverviewReport));
	EXPECT_TRUE(purposes.contains(RedmineClient::ExportReport));
}

TEST_F(RedmineClientIntegrationTest, FailedReportIsReported)
{
	server->enqueueResponse(MockHttpResponse { 500, "Internal Server Error", "" });

	int failedPurpose = -1;
	bool errorFired = false;
	QObject::connect(client.get(), &RedmineClient::timeEntryReportFailed,
		[&](const QString&, const QString&, int purpose) { failedPurpose = purpose; });
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { errorFired = true; });

	client->getTimeEntryReport("2026-10-12", "2026-10-18", RedmineClient::ExportReport);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryReportFailed));
	EXPECT_EQ(failedPurpose, RedmineClient::ExportReport);
	EXPECT_FALSE(errorFired);
}

TEST_F(RedmineClientIntegrationTest, LoadingStaysOnAcrossPagesAndEndsWithTheLastRequest)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"projects":[{"id":1}],"total_count":2,"offset":0})" });
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"projects":[{"id":2}],"total_count":2,"offset":1})" });

	QList<bool> loadingStates;
	QObject::connect(client.get(), &RedmineClient::loadingChanged, [&]() { loadingStates.append(client->loading()); });

	client->getAllProjects();
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::projectsReceived));
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));

	// On once, off once: not off and on again between the pages.
	EXPECT_EQ(loadingStates, (QList<bool> { true, false }));
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

TEST_F(RedmineClientIntegrationTest, DeletingATimeEntryThatIsGoneAlreadyCountsAsDeleted)
{
	server->enqueueResponse(MockHttpResponse { 404, "Not Found", "" });

	int deletedId = -1;
	bool errorFired = false;
	QObject::connect(client.get(), &RedmineClient::timeEntryDeleted, [&](int id) { deletedId = id; });
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { errorFired = true; });

	client->deleteTimeEntry(78);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryDeleted));

	EXPECT_EQ(deletedId, 78);
	EXPECT_FALSE(errorFired);
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
	EXPECT_EQ(request.query.queryItemValue("limit").toStdString(), "1");
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

TEST_F(RedmineClientIntegrationTest, GetIssueCanIncludeTheAllowedStatuses)
{
	server->enqueueResponse(MockHttpResponse { 200, "OK",
		R"({"issue":{"id":7,"status":{"id":2},"allowed_statuses":[{"id":2,"name":"In Progress"},{"id":5,"name":"Closed"}]}})" });

	QJsonObject received;
	QObject::connect(client.get(), &RedmineClient::issueReceived, [&](const QJsonObject& issue) { received = issue; });

	client->getIssue(7, true, true);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issueReceived));

	EXPECT_EQ(server->requests().first().query.queryItemValue("include").toStdString(), "allowed_statuses");
	EXPECT_EQ(received["allowed_statuses"].toArray().size(), 2);
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

TEST_F(RedmineClientIntegrationTest, NoErrorIsReportedWhileNoServerIsSetUp)
{
	client->setBaseUrl(QString());

	bool errorFired = false;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { errorFired = true; });

	client->getAllProjects();
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));
	waitUntil([]() { return false; }, 200);

	EXPECT_FALSE(errorFired);
}

TEST_F(RedmineClientIntegrationTest, UnknownIssueEmitsIssueNotFound)
{
	MockHttpResponse notFound { 404, "Not Found", "" };
	notFound.headers.insert("Content-Type", "application/json"); // Redmine's own answer
	server->enqueueResponse(notFound);

	int missing = -1;
	QObject::connect(client.get(), &RedmineClient::issueNotFound, [&](int issueId) { missing = issueId; });

	client->getIssue(15);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issueNotFound));
	EXPECT_EQ(missing, 15);
}

TEST_F(RedmineClientIntegrationTest, NotFoundPageFromSomethingElseSaysNothingAboutTheIssue)
{
	// E.g. a wrong server URL or a proxy answering with an HTML 404 page.
	MockHttpResponse page { 404, "Not Found", "<html>Not here</html>" };
	page.headers.insert("Content-Type", "text/html");
	server->enqueueResponse(page);

	bool notFound = false;
	QObject::connect(client.get(), &RedmineClient::issueNotFound, [&](int) { notFound = true; });

	client->getIssue(15);
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));
	waitUntil([]() { return false; }, 200);
	EXPECT_FALSE(notFound);
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
	const RefusedPort refused;
	client->setBaseUrl(refused.baseUrl());

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

namespace {
/*! \brief probe Triggers the private checkConnection() slot and waits until the server has seen the
 * request and the client has handled the reply.
 * \return Whether both happened in time */
[[nodiscard]] bool probe(RedmineClient& client, QRedmineTesting::MockHttpServer& server)
{
	const auto expected = server.requests().size() + 1;
	QMetaObject::invokeMethod(&client, "checkConnection");
	return waitUntil([&]() { return server.requests().size() >= expected; }) &&
		waitUntil([&]() { return !client.connectionCheckRunning(); });
}

/*! \brief A TCP server accepting connections but never answering them. */
class SilentServer {
public:
	SilentServer()
	{
		mServer.listen(QHostAddress::LocalHost, 0);
		QObject::connect(&mServer, &QTcpServer::newConnection, [this]() {
			while (mServer.hasPendingConnections()) {
				mServer.nextPendingConnection()->setParent(&mServer);
				++mConnections;
			}
		});
	}

	QString baseUrl() const { return QStringLiteral("http://127.0.0.1:%0/").arg(mServer.serverPort()); }

	int connections() const { return mConnections; }

private:
	QTcpServer mServer;
	int mConnections = 0;
};
}

TEST_F(RedmineClientIntegrationTest, ConnectionCheckRequestsOneIssueOnly)
{
	ASSERT_TRUE(probe(*client, *server));

	ASSERT_EQ(server->requests().size(), 1);
	const RecordedHttpRequest& request = server->requests().first();
	EXPECT_EQ(request.path.toStdString(), "/issues.json");
	EXPECT_EQ(request.query.queryItemValue("limit").toStdString(), "1");
}

TEST_F(RedmineClientIntegrationTest, SuccessfulConnectionCheckGoesOnlineAndStaysQuiet)
{
	int issuesReceived = 0;
	int errors = 0;
	std::vector<bool> loadingChanges;
	QObject::connect(client.get(), &RedmineClient::issuesReceived, [&](const QJsonArray&) { ++issuesReceived; });
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { ++errors; });
	QObject::connect(
		client.get(), &RedmineClient::loadingChanged, [&]() { loadingChanges.push_back(client->loading()); });
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issues":[{"id":1}]})" });

	ASSERT_TRUE(probe(*client, *server));

	EXPECT_EQ(client->connectionState(), RedmineClient::ONLINE);
	EXPECT_EQ(client->connectionCheckInterval(), 60000);
	EXPECT_EQ(issuesReceived, 0);
	EXPECT_EQ(errors, 0);
	EXPECT_TRUE(loadingChanges.empty());
	EXPECT_FALSE(client->loading());
}

TEST_F(RedmineClientIntegrationTest, FailingConnectionChecksBackOffUpToTheCap)
{
	int errors = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { ++errors; });

	// A success first, so that the first failure's interval differs from the one before it
	ASSERT_TRUE(probe(*client, *server));
	ASSERT_EQ(client->connectionCheckInterval(), 60000);

	for (int i = 0; i < 9; ++i) {
		server->enqueueResponse(MockHttpResponse { 503, "Service Unavailable", "" });
	}
	for (const int expected : { 5000, 10000, 20000, 40000, 80000, 160000, 300000, 300000, 300000 }) {
		ASSERT_TRUE(probe(*client, *server));
		ASSERT_EQ(client->connectionCheckInterval(), expected);
	}
	EXPECT_EQ(errors, 0);
}

TEST_F(RedmineClientIntegrationTest, ConnectionCheckAgainstClosedPortBacksOffWithoutError)
{
	const RefusedPort refused;
	int errors = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { ++errors; });

	client->setBaseUrl(refused.baseUrl());
	for (const int expected : { 5000, 10000 }) {
		QMetaObject::invokeMethod(client.get(), "checkConnection");
		ASSERT_TRUE(waitUntil([&]() { return !client->connectionCheckRunning(); }));
		EXPECT_EQ(client->connectionCheckInterval(), expected);
	}
	// Refused isn't a lost connection for other requests, but a failed check means one
	EXPECT_NE(client->connectionState(), RedmineClient::ONLINE);
	EXPECT_EQ(errors, 0);
	EXPECT_FALSE(client->loading());
}

TEST_F(RedmineClientIntegrationTest, ConnectionCheckIsNotSentWhileOneIsPending)
{
	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());

	QMetaObject::invokeMethod(client.get(), "checkConnection");
	ASSERT_TRUE(waitUntil([&]() { return silent.connections() == 1; }));
	QMetaObject::invokeMethod(client.get(), "checkConnection");
	QMetaObject::invokeMethod(client.get(), "checkConnection");
	waitUntil([]() { return false; }, 200);

	EXPECT_EQ(silent.connections(), 1);
	EXPECT_FALSE(client->loading());
}

TEST_F(RedmineClientIntegrationTest, SuccessfulRequestAfterFailedConnectionChecksResetsTheBackoff)
{
	for (int i = 0; i < 3; ++i) {
		server->enqueueResponse(MockHttpResponse { 503, "Service Unavailable", "" });
	}
	ASSERT_TRUE(probe(*client, *server));
	ASSERT_TRUE(probe(*client, *server));
	ASSERT_TRUE(probe(*client, *server));
	ASSERT_EQ(client->connectionCheckInterval(), 20000);

	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issue":{"id":1}})" });
	client->getIssue(1);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issueReceived));

	EXPECT_EQ(client->connectionCheckInterval(), 60000);
	EXPECT_EQ(client->connectionState(), RedmineClient::ONLINE);
}

TEST_F(RedmineClientIntegrationTest, ChangingTheUrlResetsTheBackoffAndAbortsThePendingCheck)
{
	for (int i = 0; i < 2; ++i) {
		server->enqueueResponse(MockHttpResponse { 503, "Service Unavailable", "" });
	}
	ASSERT_TRUE(probe(*client, *server));
	ASSERT_TRUE(probe(*client, *server));
	ASSERT_EQ(client->connectionCheckInterval(), 10000);

	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());
	EXPECT_EQ(client->connectionCheckInterval(), 5000);
	QMetaObject::invokeMethod(client.get(), "checkConnection");
	ASSERT_TRUE(waitUntil([&]() { return silent.connections() == 1; }));

	QTcpServer other;
	other.listen(QHostAddress::LocalHost, 0);
	client->setBaseUrl(QStringLiteral("http://127.0.0.1:%0/").arg(other.serverPort()));
	waitUntil([]() { return false; }, 100);

	EXPECT_EQ(client->connectionCheckInterval(), 5000);
	// The aborted probe is gone, so a new one is sent immediately
	QMetaObject::invokeMethod(client.get(), "checkConnection");
	EXPECT_TRUE(waitUntil([&]() { return other.hasPendingConnections(); }));
}

TEST_F(RedmineClientIntegrationTest, TransferTimeoutIsThirtySeconds)
{
	auto* manager = client->findChild<QNetworkAccessManager*>();
	ASSERT_NE(manager, nullptr);
	EXPECT_EQ(manager->transferTimeout(), 30000);
}

TEST_F(RedmineClientIntegrationTest, RequestToSilentServerIsCanceledByTheTransferTimeout)
{
	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());
	// Shortened from the test only, the production value is 30 s (see TransferTimeoutIsThirtySeconds)
	client->findChild<QNetworkAccessManager*>()->setTransferTimeout(std::chrono::milliseconds(300));

	QString message;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString& text) { message = text; });

	client->getIssue(1);
	EXPECT_TRUE(client->loading());
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));

	EXPECT_EQ(silent.connections(), 1);
	EXPECT_EQ(client->connectionState(), RedmineClient::OFFLINE);
	EXPECT_TRUE(message.contains("didn't answer in time")) << message.toStdString();
}

TEST_F(RedmineClientIntegrationTest, ClearingTheUrlStopsTheConnectionChecks)
{
	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());
	QMetaObject::invokeMethod(client.get(), "checkConnection");
	ASSERT_TRUE(waitUntil([&]() { return client->connectionCheckRunning(); }));

	client->setBaseUrl(QString());

	EXPECT_FALSE(client->connectionCheckRunning());
	QMetaObject::invokeMethod(client.get(), "checkConnection");
	EXPECT_FALSE(client->connectionCheckRunning());
}

namespace {
/*! \brief pause Lets the event loop run for ms, to show that something does not happen. */
void quietFor(int ms = 200)
{
	waitUntil([]() { return false; }, ms);
}

/*! \brief pagedResponse A page of one project, of a list claiming to hold a lot more. */
MockHttpResponse endlessPage(const char* key)
{
	return MockHttpResponse { 200, "OK", QByteArray("{\"") + key + "\":[{\"id\":1}],\"total_count\":100000}" };
}
}

TEST_F(RedmineClientIntegrationTest, PagingAdvancesFromTheRequestedOffsetNotTheReportedOne)
{
	// The server leaves the offset out, or always reports 0
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"projects":[{"id":1},{"id":2}],"total_count":3})" });
	server->enqueueResponse(
		MockHttpResponse { 200, "OK", R"({"projects":[{"id":3}],"total_count":3,"offset":0,"limit":100})" });

	QJsonArray received;
	QObject::connect(
		client.get(), &RedmineClient::projectsReceived, [&](const QJsonArray& projects) { received = projects; });

	client->getAllProjects();
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::projectsReceived));
	quietFor();

	ASSERT_EQ(server->requests().size(), 2);
	EXPECT_EQ(server->requests().at(0).query.queryItemValue("offset").toStdString(), "0");
	EXPECT_EQ(server->requests().at(1).query.queryItemValue("offset").toStdString(), "2");
	EXPECT_EQ(received.size(), 3);
}

TEST_F(RedmineClientIntegrationTest, PagedFetchStopsAfterTwoHundredPages)
{
	for (int i = 0; i < 250; ++i) {
		server->enqueueResponse(endlessPage("projects"));
	}

	QJsonArray received;
	QObject::connect(
		client.get(), &RedmineClient::projectsReceived, [&](const QJsonArray& projects) { received = projects; });

	client->getAllProjects();
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::projectsReceived, 20000));
	quietFor();

	EXPECT_EQ(server->requests().size(), 200);
	EXPECT_EQ(received.size(), 200);
	EXPECT_FALSE(client->loading());
}

TEST_F(RedmineClientIntegrationTest, OwnTimeEntriesCutOffAtTheLimitAreNotEmitted)
{
	for (int i = 0; i < 250; ++i) {
		server->enqueueResponse(endlessPage("time_entries"));
	}

	int received = 0;
	QObject::connect(client.get(), &RedmineClient::ownTimeEntriesReceived, [&](const QJsonArray&) { ++received; });

	client->getOwnTimeEntries("2026-01-01", "2026-12-31");
	ASSERT_TRUE(waitUntil([&]() { return server->requests().size() == 200 && !client->loading(); }, 20000));
	quietFor();

	// A truncated list would have pending entries submitted again
	EXPECT_EQ(server->requests().size(), 200);
	EXPECT_EQ(received, 0);
}

TEST_F(RedmineClientIntegrationTest, ReportCutOffAtTheLimitFailsInsteadOfBeingPartial)
{
	for (int i = 0; i < 250; ++i) {
		server->enqueueResponse(endlessPage("time_entries"));
	}

	bool received = false;
	QString failedFrom;
	int failedPurpose = -1;
	QObject::connect(client.get(), &RedmineClient::timeEntryReportReceived,
		[&](const QJsonArray&, const QString&, const QString&, int) { received = true; });
	QObject::connect(
		client.get(), &RedmineClient::timeEntryReportFailed, [&](const QString& from, const QString&, int purpose) {
			failedFrom = from;
			failedPurpose = purpose;
		});

	client->getTimeEntryReport("2026-01-01", "2026-12-31", RedmineClient::ExportReport);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryReportFailed, 20000));
	quietFor();

	EXPECT_EQ(server->requests().size(), 200);
	EXPECT_FALSE(received);
	EXPECT_EQ(failedFrom.toStdString(), "2026-01-01");
	EXPECT_EQ(failedPurpose, RedmineClient::ExportReport);
	EXPECT_FALSE(client->loading());
}

TEST_F(RedmineClientIntegrationTest, ReportOfExactlyTwoHundredPagesIsComplete)
{
	for (int i = 0; i < 200; ++i) {
		server->enqueueResponse(
			MockHttpResponse { 200, "OK", R"({"time_entries":[{"id":1}],"total_count":200,"offset":0})" });
	}

	QJsonArray received;
	bool failed = false;
	QObject::connect(client.get(), &RedmineClient::timeEntryReportReceived,
		[&](const QJsonArray& entries, const QString&, const QString&, int) { received = entries; });
	QObject::connect(client.get(), &RedmineClient::timeEntryReportFailed,
		[&](const QString&, const QString&, int) { failed = true; });

	client->getTimeEntryReport("2026-01-01", "2026-12-31");
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::timeEntryReportReceived, 20000));

	EXPECT_EQ(received.size(), 200);
	EXPECT_FALSE(failed);
	EXPECT_EQ(server->requests().size(), 200);
}

TEST_F(RedmineClientIntegrationTest, ChangingTheUrlAbortsPendingRequestSilently)
{
	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());

	int errors = 0;
	int issues = 0;
	int stateChanges = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { ++errors; });
	QObject::connect(client.get(), &RedmineClient::issueReceived, [&](const QJsonObject&) { ++issues; });
	QObject::connect(client.get(), &RedmineClient::issueNotFound, [&](int) { ++issues; });
	QObject::connect(client.get(), &RedmineClient::connectionStateChanged, [&]() { ++stateChanges; });

	client->getIssue(1);
	ASSERT_TRUE(waitUntil([&]() { return silent.connections() == 1; }));
	ASSERT_TRUE(client->loading());

	client->setBaseUrl(server->baseUrl());
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));
	quietFor();

	EXPECT_EQ(errors, 0);
	EXPECT_EQ(issues, 0);
	EXPECT_EQ(stateChanges, 0);
	EXPECT_FALSE(client->loading());
}

TEST_F(RedmineClientIntegrationTest, ChangingTheKeyAbortsPendingRequestSilently)
{
	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());

	int errors = 0;
	int received = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { ++errors; });
	QObject::connect(client.get(), &RedmineClient::projectReceived, [&](const QJsonObject&) { ++received; });

	client->getProject(3);
	ASSERT_TRUE(waitUntil([&]() { return silent.connections() == 1; }));
	client->setApiKey("another-key");
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));
	quietFor();

	EXPECT_EQ(errors, 0);
	EXPECT_EQ(received, 0);
}

TEST_F(RedmineClientIntegrationTest, TimeEntryCreationIsNotAbortedByAChange)
{
	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());

	int failures = 0;
	int errors = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { ++errors; });
	QObject::connect(
		client.get(), &RedmineClient::timeEntryCreateFailed, [&](int, int, const QString&, int) { ++failures; });

	client->createTimeEntry(42, 1.0, "Pending", "2026-10-15", 4, {}, 7);
	ASSERT_TRUE(waitUntil([&]() { return silent.connections() == 1; }));
	client->setApiKey("another-key");
	quietFor();

	// Still running: the server may have it already, so it must not be reported as failed
	EXPECT_EQ(failures, 0);
	EXPECT_EQ(errors, 0);
	EXPECT_TRUE(client->loading());
}

TEST_F(RedmineClientIntegrationTest, TimeEntryUpdateIsNotAbortedByAChange)
{
	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());

	int failures = 0;
	QObject::connect(client.get(), &RedmineClient::timeEntryUpdateFailed, [&](int, int) { ++failures; });

	client->updateTimeEntry(9, 2.0, "Changed", "2026-10-15", 4, {}, 5);
	ASSERT_TRUE(waitUntil([&]() { return silent.connections() == 1; }));
	client->setBaseUrl(server->baseUrl());
	quietFor();

	EXPECT_EQ(failures, 0);
	EXPECT_TRUE(client->loading());
}

TEST_F(RedmineClientIntegrationTest, AbortedReportFetchIsReportedAsFailed)
{
	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());

	int received = 0;
	QString failedFrom;
	QString failedTo;
	int failedPurpose = -1;
	int failures = 0;
	QObject::connect(client.get(), &RedmineClient::timeEntryReportReceived,
		[&](const QJsonArray&, const QString&, const QString&, int) { ++received; });
	QObject::connect(
		client.get(), &RedmineClient::timeEntryReportFailed, [&](const QString& from, const QString& to, int purpose) {
			++failures;
			failedFrom = from;
			failedTo = to;
			failedPurpose = purpose;
		});

	client->getTimeEntryReport("2026-01-01", "2026-12-31", RedmineClient::ExportReport);
	ASSERT_TRUE(waitUntil([&]() { return silent.connections() == 1; }));
	client->setBaseUrl(server->baseUrl());
	ASSERT_TRUE(waitUntil([&]() { return failures > 0; }));
	quietFor();

	EXPECT_EQ(failures, 1);
	EXPECT_EQ(received, 0);
	EXPECT_EQ(failedFrom.toStdString(), "2026-01-01");
	EXPECT_EQ(failedTo.toStdString(), "2026-12-31");
	EXPECT_EQ(failedPurpose, RedmineClient::ExportReport);
	EXPECT_FALSE(client->loading());
}

TEST_F(RedmineClientIntegrationTest, AbortedPagedFetchStopsPaging)
{
	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());

	int received = 0;
	QObject::connect(client.get(), &RedmineClient::projectsReceived, [&](const QJsonArray&) { ++received; });

	client->getAllProjects();
	ASSERT_TRUE(waitUntil([&]() { return silent.connections() == 1; }));
	client->setBaseUrl(server->baseUrl());
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));
	quietFor();

	EXPECT_EQ(received, 0);
	EXPECT_TRUE(server->requests().isEmpty());
}

TEST_F(RedmineClientIntegrationTest, RequestsAfterAChangeAreNotAffectedByTheAbort)
{
	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());
	client->getIssue(1);
	ASSERT_TRUE(waitUntil([&]() { return silent.connections() == 1; }));

	client->setBaseUrl(server->baseUrl());
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issue":{"id":2}})" });
	QJsonObject received;
	QObject::connect(client.get(), &RedmineClient::issueReceived, [&](const QJsonObject& issue) { received = issue; });
	client->getIssue(2);

	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issueReceived));
	EXPECT_EQ(received["id"].toInt(), 2);
	EXPECT_EQ(client->connectionState(), RedmineClient::ONLINE);
}

TEST_F(RedmineClientIntegrationTest, InvalidEnumerationTypeDoesNotSetLoading)
{
	int loadingChanges = 0;
	QObject::connect(client.get(), &RedmineClient::loadingChanged, [&]() { ++loadingChanges; });

	client->getEnumerations(static_cast<RedmineClient::EnumerationTypes>(99));
	quietFor(100);

	EXPECT_FALSE(client->loading());
	EXPECT_EQ(loadingChanges, 0);
	EXPECT_TRUE(server->requests().isEmpty());
}

namespace {
struct CreateFailure {
	int status = -1;
	int retryAfter = -1;
};

/*! \brief failCreateWith Answers a createTimeEntry() request with the given response, returns what
 * timeEntryCreateFailed() reported. */
CreateFailure failCreateWith(RedmineClient& client, QRedmineTesting::MockHttpServer& server, MockHttpResponse response)
{
	server.enqueueResponse(response);
	CreateFailure failure;
	// Disconnected on return (context): the lambda writes into this call's stack, which a later call's
	// signal would otherwise still reach.
	QObject context;
	QObject::connect(
		&client, &RedmineClient::timeEntryCreateFailed, &context, [&](int, int status, const QString&, int retryAfter) {
			failure = { status, retryAfter };
		});
	client.createTimeEntry(42, 1.0, "Retry", "2026-10-15", 4, {}, 1);
	EXPECT_TRUE(waitForSignal(&client, &RedmineClient::timeEntryCreateFailed));
	return failure;
}

MockHttpResponse retryResponse(int status, const QByteArray& retryAfter)
{
	MockHttpResponse response { status, "Error", R"({"errors":["later"]})" };
	if (!retryAfter.isNull()) {
		response.headers.insert("Retry-After", retryAfter);
	}
	return response;
}

//! A date as in an HTTP header, ending in zone ("GMT" by the standard)
QByteArray httpDate(qint64 secondsFromNow, const QString& zone = QStringLiteral("GMT"))
{
	return (
		QLocale::c().toString(QDateTime::currentDateTimeUtc().addSecs(secondsFromNow), "ddd, dd MMM yyyy HH:mm:ss ") +
		zone)
		.toLatin1();
}
}

TEST_F(RedmineClientIntegrationTest, RetryAfterInSecondsIsPassedOn)
{
	const auto failure = failCreateWith(*client, *server, retryResponse(503, "120"));
	EXPECT_EQ(failure.status, 503);
	EXPECT_EQ(failure.retryAfter, 120);
}

TEST_F(RedmineClientIntegrationTest, TimeEntryCreationAnsweredByAGatewayErrorIsNotReported)
{
	int errors = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { ++errors; });

	const auto failure = failCreateWith(*client, *server, retryResponse(502, QByteArray()));
	quietFor();

	EXPECT_EQ(failure.status, 502);
	EXPECT_EQ(errors, 0);
}

TEST_F(RedmineClientIntegrationTest, RetryAfterAsHttpDateWithNumericZoneIsConvertedToSeconds)
{
	const auto failure = failCreateWith(*client, *server, retryResponse(429, httpDate(300, "+0000")));
	EXPECT_EQ(failure.status, 429);
	EXPECT_GE(failure.retryAfter, 290);
	EXPECT_LE(failure.retryAfter, 300);
}

// The form servers send: HTTP dates end in "GMT" ("Wed, 21 Oct 2026 07:28:00 GMT"), which
// Qt::RFC2822Date alone doesn't parse
TEST_F(RedmineClientIntegrationTest, RetryAfterAsHttpDateIsConvertedToSeconds)
{
	const auto failure = failCreateWith(*client, *server, retryResponse(429, httpDate(300)));
	EXPECT_EQ(failure.status, 429);
	EXPECT_GE(failure.retryAfter, 290);
	EXPECT_LE(failure.retryAfter, 300);
}

TEST_F(RedmineClientIntegrationTest, MissingRetryAfterIsZero)
{
	const auto failure = failCreateWith(*client, *server, retryResponse(503, QByteArray()));
	EXPECT_EQ(failure.status, 503);
	EXPECT_EQ(failure.retryAfter, 0);
}

TEST_F(RedmineClientIntegrationTest, RetryAfterIsClampedToOneDay)
{
	EXPECT_EQ(failCreateWith(*client, *server, retryResponse(503, "999999")).retryAfter, 86400);
}

TEST_F(RedmineClientIntegrationTest, RetryAfterInTheNegativeOrPastOrNonsenseIsZero)
{
	EXPECT_EQ(failCreateWith(*client, *server, retryResponse(503, "-5")).retryAfter, 0);
	EXPECT_EQ(failCreateWith(*client, *server, retryResponse(503, httpDate(-600, "+0000"))).retryAfter, 0);
	EXPECT_EQ(failCreateWith(*client, *server, retryResponse(503, "soon")).retryAfter, 0);
}

TEST_F(RedmineClientIntegrationTest, GetIssuesByIdSplitsIdsIntoChunksOfOneHundred)
{
	QList<int> ids;
	for (int i = 1; i <= 150; ++i) {
		ids.append(i);
	}

	client->getIssuesById(ids);
	ASSERT_TRUE(waitUntil([&]() { return server->requests().size() == 2 && !client->loading(); }));

	QList<qsizetype> sizes;
	QList<int> requested;
	for (const RecordedHttpRequest& request : server->requests()) {
		EXPECT_EQ(request.method.toStdString(), "GET");
		EXPECT_EQ(request.path.toStdString(), "/issues.json");
		EXPECT_EQ(request.query.queryItemValue("status_id").toStdString(), "*");
		EXPECT_EQ(request.query.queryItemValue("limit").toStdString(), "100");
		const QStringList chunk = request.query.queryItemValue("issue_id").split(',');
		sizes.append(chunk.size());
		for (const QString& id : chunk) {
			requested.append(id.toInt());
		}
	}
	std::sort(sizes.begin(), sizes.end());
	std::sort(requested.begin(), requested.end());
	EXPECT_EQ(sizes, (QList<qsizetype> { 50, 100 }));
	EXPECT_EQ(requested, ids);
}

TEST_F(RedmineClientIntegrationTest, GetIssuesByIdIgnoresDuplicatesAndInvalidIds)
{
	client->getIssuesById({ 3, 3, 0, -1, 5, 3, 5 });
	ASSERT_TRUE(waitUntil([&]() { return server->requests().size() == 1 && !client->loading(); }));
	quietFor(100);

	ASSERT_EQ(server->requests().size(), 1);
	EXPECT_EQ(server->requests().first().query.queryItemValue("issue_id").toStdString(), "3,5");
}

TEST_F(RedmineClientIntegrationTest, GetIssuesByIdWithoutValidIdsSendsNothing)
{
	client->getIssuesById({});
	client->getIssuesById({ 0, -4 });
	quietFor(100);

	EXPECT_TRUE(server->requests().isEmpty());
	EXPECT_FALSE(client->loading());
}

TEST_F(RedmineClientIntegrationTest, GetIssuesByIdEmitsIssueReceivedForEachIssueReturned)
{
	QList<int> ids;
	for (int i = 1; i <= 120; ++i) {
		ids.append(i);
	}
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issues":[{"id":1},{"id":2}]})" });
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issues":[{"id":101}]})" });

	QList<int> received;
	int lists = 0;
	int notFound = 0;
	QObject::connect(client.get(), &RedmineClient::issueReceived,
		[&](const QJsonObject& issue) { received.append(issue["id"].toInt()); });
	QObject::connect(client.get(), &RedmineClient::issuesReceived, [&](const QJsonArray&) { ++lists; });
	QObject::connect(client.get(), &RedmineClient::issueNotFound, [&](int) { ++notFound; });

	client->getIssuesById(ids);
	ASSERT_TRUE(waitUntil([&]() { return received.size() == 3; }));
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));
	quietFor(100);

	std::sort(received.begin(), received.end());
	EXPECT_EQ(received, (QList<int> { 1, 2, 101 }));
	EXPECT_EQ(lists, 0);
	EXPECT_EQ(notFound, 0);
}

TEST_F(RedmineClientIntegrationTest, GetIssuesByIdHasAtMostTwoRequestsRunning)
{
	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());
	QList<int> ids;
	for (int i = 1; i <= 350; ++i) {
		ids.append(i);
	}

	client->getIssuesById(ids);
	ASSERT_TRUE(waitUntil([&]() { return silent.connections() == 2; }));
	quietFor();

	EXPECT_EQ(silent.connections(), 2);
}

TEST_F(RedmineClientIntegrationTest, GetIssuesByIdSendsTheNextChunkWhenOneIsDone)
{
	QList<int> ids;
	for (int i = 1; i <= 350; ++i) {
		ids.append(i);
	}
	int received = 0;
	QObject::connect(client.get(), &RedmineClient::issueReceived, [&](const QJsonObject&) { ++received; });
	for (int i = 0; i < 4; ++i) {
		server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issues":[{"id":1}]})" });
	}

	client->getIssuesById(ids);

	ASSERT_TRUE(waitUntil([&]() { return received == 4; }));
	EXPECT_EQ(server->requests().size(), 4);
}

TEST_F(RedmineClientIntegrationTest, GetIssuesByIdIsQuietByDefaultAndReportsErrorsOnRequest)
{
	int errors = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, [&](const QString&) { ++errors; });
	server->enqueueResponse(MockHttpResponse { 500, "Internal Server Error", "" });
	server->enqueueResponse(MockHttpResponse { 500, "Internal Server Error", "" });

	client->getIssuesById({ 1 });
	ASSERT_TRUE(waitUntil([&]() { return server->requests().size() == 1 && !client->loading(); }));
	quietFor(100);
	EXPECT_EQ(errors, 0);

	client->getIssuesById({ 1 }, false);
	ASSERT_TRUE(waitUntil([&]() { return errors == 1; }));
}

TEST_F(RedmineClientIntegrationTest, ChangingTheUrlDropsQueuedIssueChunksAndLetsNewOnesThrough)
{
	SilentServer silent;
	client->setBaseUrl(silent.baseUrl());
	QList<int> ids;
	for (int i = 1; i <= 500; ++i) {
		ids.append(i);
	}
	client->getIssuesById(ids);
	ASSERT_TRUE(waitUntil([&]() { return silent.connections() == 2; }));

	client->setBaseUrl(server->baseUrl());
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));
	quietFor();
	EXPECT_TRUE(server->requests().isEmpty());

	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issues":[{"id":9}]})" });
	client->getIssuesById({ 9 });
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issueReceived));
	EXPECT_EQ(server->requests().size(), 1);
}

TEST_F(RedmineClientIntegrationTest, ApiKeyIsLoadedAfterSettingIt)
{
	RedmineClient fresh;
	// The keychain read hasn't been delivered before the constructor returns
	EXPECT_FALSE(fresh.apiKeyLoaded());

	int changes = 0;
	QObject::connect(&fresh, &RedmineClient::apiKeyLoadedChanged, [&]() { ++changes; });
	fresh.setApiKey("my-key");
	EXPECT_TRUE(fresh.apiKeyLoaded());
	EXPECT_EQ(changes, 1);

	fresh.setApiKey("other-key");
	quietFor(100);
	EXPECT_EQ(changes, 1);
	EXPECT_EQ(fresh.apiKey().toStdString(), "other-key"); // not replaced by what the keychain delivers
}

TEST_F(RedmineClientIntegrationTest, ApiKeyIsLoadedOnceTheKeychainReadIsFinished)
{
	RedmineClient fresh;
	int changes = 0;
	QObject::connect(&fresh, &RedmineClient::apiKeyLoadedChanged, [&]() { ++changes; });

	// Whether it finds a key, none or fails (no secret service here)
	ASSERT_TRUE(waitUntil([&]() { return fresh.apiKeyLoaded(); }, 10000));
	quietFor(100);
	EXPECT_EQ(changes, 1);
}

TEST_F(RedmineClientIntegrationTest, MigratingALegacyKeyMarksItLoadedAtOnce)
{
	QSettings().setValue("client/apiKey", "legacy-key");

	RedmineClient fresh;

	EXPECT_TRUE(fresh.apiKeyLoaded());
	EXPECT_EQ(fresh.apiKey().toStdString(), "legacy-key");
}

namespace {
//! Makes the client ONLINE with a successful request, then points it to a URL connections are refused on.
void goOnlineThenUnreachable(QRedmineTesting::MockHttpServer& server, RedmineClient& client, const RefusedPort& refused)
{
	server.enqueueResponse(MockHttpResponse { 200, "OK", R"({"issue":{"id":1}})" });
	client.getIssue(1);
	ASSERT_TRUE(waitForSignal(&client, &RedmineClient::issueReceived));
	ASSERT_EQ(client.connectionState(), RedmineClient::ONLINE);
	client.setBaseUrl(refused.baseUrl());
	ASSERT_EQ(client.connectionState(), RedmineClient::ONLINE);
}
}

TEST_F(RedmineClientIntegrationTest, StartupRequestsDontReportAnUnreachableServer)
{
	const RefusedPort refused;
	goOnlineThenUnreachable(*server, *client, refused);
	ASSERT_FALSE(HasFatalFailure());
	QObject context;
	int errors = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, &context, [&](const QString&) { ++errors; });

	client->detectApiVersion();
	client->getEnumerations(RedmineClient::TIME_ENTRY_ACTIVITIES);
	client->getAllProjects();
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));

	// The connection state shows it instead
	EXPECT_EQ(errors, 0);
	EXPECT_EQ(client->connectionState(), RedmineClient::TEMPORARY_OFFLINE);
}

TEST_F(RedmineClientIntegrationTest, AUserRequestedProjectFetchReportsAnUnreachableServer)
{
	const RefusedPort refused;
	client->setBaseUrl(refused.baseUrl());
	QObject context;
	int errors = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, &context, [&](const QString&) { ++errors; });

	client->getAllProjects(true);
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));

	EXPECT_EQ(errors, 1);
}

TEST_F(RedmineClientIntegrationTest, AUserActionReportsAnUnreachableServer)
{
	const RefusedPort refused;
	client->setBaseUrl(refused.baseUrl());
	QObject context;
	int errors = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, &context, [&](const QString&) { ++errors; });

	client->getTimeEntries(1);
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));

	EXPECT_EQ(errors, 1);
}

TEST_F(RedmineClientIntegrationTest, StartupRequestsStillReportServerErrors)
{
	QObject context;
	int errors = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, &context, [&](const QString&) { ++errors; });
	server->enqueueResponse(MockHttpResponse { 401, "Unauthorized", "" });

	client->detectApiVersion();
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));

	// A wrong API key is something the user can and needs to fix
	EXPECT_EQ(errors, 1);
}

TEST_F(RedmineClientIntegrationTest, AGatewayErrorOnAStartupRequestIsntReportedAndMarksTheClientOffline)
{
	// Online first, as a client that never was online stays OFFLINE
	server->enqueueResponse(MockHttpResponse { 200, "OK", R"({"issue":{"id":1}})" });
	client->getIssue(1);
	ASSERT_TRUE(waitForSignal(client.get(), &RedmineClient::issueReceived));
	ASSERT_EQ(client->connectionState(), RedmineClient::ONLINE);
	QObject context;
	int errors = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, &context, [&](const QString&) { ++errors; });
	server->enqueueResponse(MockHttpResponse { 502, "Bad Gateway", "" });

	client->detectApiVersion();
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));

	EXPECT_EQ(errors, 0);
	EXPECT_EQ(client->connectionState(), RedmineClient::TEMPORARY_OFFLINE);
}

TEST_F(RedmineClientIntegrationTest, AnInternalServerErrorOnAStartupRequestIsReported)
{
	QObject context;
	int errors = 0;
	QObject::connect(client.get(), &RedmineClient::errorOccurred, &context, [&](const QString&) { ++errors; });
	server->enqueueResponse(MockHttpResponse { 500, "Internal Server Error", "" });

	client->detectApiVersion();
	ASSERT_TRUE(waitUntil([&]() { return !client->loading(); }));

	EXPECT_EQ(errors, 1);
}

TEST_F(RedmineClientIntegrationTest, ServerNotReachedClassifiesNetworkErrors)
{
	using Error = QNetworkReply::NetworkError;
	for (const Error error : { Error::ConnectionRefusedError, Error::RemoteHostClosedError, Error::HostNotFoundError,
			 Error::TimeoutError, Error::OperationCanceledError, Error::TemporaryNetworkFailureError,
			 Error::NetworkSessionFailedError, Error::UnknownNetworkError, Error::UnknownServerError,
			 Error::ServiceUnavailableError, Error::ProxyConnectionClosedError, Error::ProxyTimeoutError }) {
		EXPECT_TRUE(RedmineClient::serverNotReached(error)) << error;
		EXPECT_TRUE(RedmineClient::isConnectionLoss(error)) << error;
	}
	// Setup problems the user has to fix, and answers of the server
	for (const Error error : { Error::NoError, Error::SslHandshakeFailedError, Error::TooManyRedirectsError,
			 Error::InsecureRedirectError, Error::ProxyConnectionRefusedError, Error::ProxyNotFoundError,
			 Error::ProxyAuthenticationRequiredError, Error::ContentNotFoundError, Error::ContentAccessDenied,
			 Error::InternalServerError, Error::ProtocolFailure }) {
		EXPECT_FALSE(RedmineClient::serverNotReached(error)) << error;
		EXPECT_FALSE(RedmineClient::isConnectionLoss(error)) << error;
	}
	EXPECT_FALSE(RedmineClient::serverNotReached(Error::AuthenticationRequiredError));
	EXPECT_TRUE(RedmineClient::isConnectionLoss(Error::AuthenticationRequiredError));
}
