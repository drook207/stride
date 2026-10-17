#include "projectaccess.h"
#include <QJsonDocument>
#include <gtest/gtest.h>

using QRedmineClient::ProjectAccess;

namespace {

QJsonArray json(const char* text) { return QJsonDocument::fromJson(text).array(); }

QJsonObject object(const char* text) { return QJsonDocument::fromJson(text).object(); }

//! Ids of the projects ProjectAccess offers.
std::vector<int> offeredIds(const ProjectAccess& access)
{
	std::vector<int> ids;
	for (const QVariant& project : access.issueProjects()) {
		ids.push_back(project.toMap()["id"].toInt());
	}
	return ids;
}

// 1: public, user is a Developer; 2: public, user isn't a member; 3: private, user isn't a member;
// 4: closed; 5: public without issue tracking.
const char* PROJECTS = R"([
	{"id":1,"status":1,"is_public":true,"enabled_modules":[{"name":"issue_tracking"}]},
	{"id":2,"status":1,"is_public":true,"enabled_modules":[{"name":"issue_tracking"}]},
	{"id":3,"status":1,"is_public":false,"enabled_modules":[{"name":"issue_tracking"}]},
	{"id":4,"status":5,"is_public":true,"enabled_modules":[{"name":"issue_tracking"}]},
	{"id":5,"status":1,"is_public":true,"enabled_modules":[{"name":"time_tracking"}]}])";

const char* MEMBER = R"({"id":7,"memberships":[{"project":{"id":1},"roles":[{"id":4}]}]})";
const char* GIVABLE_ROLES = R"([{"id":3},{"id":4},{"id":5}])";

}

TEST(ProjectAccessTest, OffersActiveProjectsWithIssueTrackingWhileTheUserIsUnknown)
{
	ProjectAccess access;
	access.setProjects(json(PROJECTS));

	EXPECT_EQ(offeredIds(access), (std::vector<int> { 1, 2, 3 }));
}

TEST(ProjectAccessTest, FiltersByMembershipRolesAndTheNonMemberRole)
{
	ProjectAccess access;
	access.setProjects(json(PROJECTS));
	access.setGivableRoles(json(GIVABLE_ROLES));
	access.setCurrentUser(object(MEMBER));
	access.setRole(object(R"({"id":4,"permissions":["add_issues","log_time"]})"));
	access.setRole(object(R"({"id":1,"permissions":["view_issues"]})"));

	EXPECT_EQ(offeredIds(access), (std::vector<int> { 1 }));
}

TEST(ProjectAccessTest, NonMemberRoleAllowsAddingIssuesToPublicProjects)
{
	ProjectAccess access;
	access.setProjects(json(PROJECTS));
	access.setGivableRoles(json(GIVABLE_ROLES));
	access.setCurrentUser(object(MEMBER));
	access.setRole(object(R"({"id":4,"permissions":["view_issues"]})"));
	access.setRole(object(R"({"id":1,"permissions":["add_issues"]})"));

	// Not project 1 (the membership's role doesn't allow it), not the private project 3.
	EXPECT_EQ(offeredIds(access), (std::vector<int> { 2 }));
}

TEST(ProjectAccessTest, Id1IsNotTakenForNonMemberWhenItIsAGivableRole)
{
	ProjectAccess access;
	access.setProjects(json(PROJECTS));
	access.setGivableRoles(json(R"([{"id":1},{"id":4}])"));
	access.setCurrentUser(object(MEMBER));
	access.setRole(object(R"({"id":4,"permissions":["add_issues"]})"));
	access.setRole(object(R"({"id":1,"permissions":["view_issues"]})"));

	// The non-member role is unknown, so public project 2 stays offered.
	EXPECT_EQ(offeredIds(access), (std::vector<int> { 1, 2 }));
}

TEST(ProjectAccessTest, AdminsMayAddIssuesEverywhere)
{
	ProjectAccess access;
	access.setProjects(json(PROJECTS));
	access.setGivableRoles(json(GIVABLE_ROLES));
	access.setCurrentUser(object(R"({"id":1,"admin":true,"memberships":[]})"));

	EXPECT_EQ(offeredIds(access), (std::vector<int> { 1, 2, 3 }));
}

TEST(ProjectAccessTest, PublishesOnceAllRequestedRolesArrived)
{
	ProjectAccess access;
	access.setProjects(json(PROJECTS));
	access.setGivableRoles(json(GIVABLE_ROLES));

	int published = 0;
	QObject::connect(&access, &ProjectAccess::issueProjectsChanged, [&]() { ++published; });

	access.setCurrentUser(object(MEMBER));
	access.setRole(object(R"({"id":1,"permissions":[]})"));
	EXPECT_EQ(published, 0); // role 4 still missing
	access.setRole(object(R"({"id":4,"permissions":["add_issues"]})"));
	EXPECT_EQ(published, 1);
	EXPECT_EQ(offeredIds(access), (std::vector<int> { 1 }));
}

TEST(ProjectAccessTest, StopWaitingPublishesWithWhatIsKnown)
{
	ProjectAccess access;
	access.setProjects(json(PROJECTS));
	access.setGivableRoles(json(GIVABLE_ROLES));
	access.setCurrentUser(object(MEMBER));
	access.setRole(object(R"({"id":1,"permissions":[]})"));
	// Role 4 failed to arrive: it counts as allowing.
	access.stopWaiting();

	EXPECT_EQ(offeredIds(access), (std::vector<int> { 1 }));
}

TEST(ProjectAccessTest, IgnoresRolesNotAskedFor)
{
	ProjectAccess access;
	access.setProjects(json(PROJECTS));
	access.setGivableRoles(json(GIVABLE_ROLES));
	access.setCurrentUser(object(MEMBER));
	access.setRole(object(R"({"id":4,"permissions":["add_issues"]})"));
	access.setRole(object(R"({"id":1,"permissions":["add_issues"]})"));
	// A late reply from an earlier lookup must not change the result.
	access.setRole(object(R"({"id":4,"permissions":[]})"));

	EXPECT_EQ(offeredIds(access), (std::vector<int> { 1, 2 }));
}
