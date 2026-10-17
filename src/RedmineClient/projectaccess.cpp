#include "projectaccess.h"

#include <QLoggingCategory>
#include <algorithm>

Q_LOGGING_CATEGORY(lcProjectAccess, "qredmine.client.projectaccess")

namespace QRedmineClient {

ProjectAccess::ProjectAccess(QObject* parent)
	: QObject(parent)
{
}

RedmineClient* ProjectAccess::client() const { return mClient; }

void ProjectAccess::setClient(RedmineClient* client)
{
	if (mClient == client) {
		return;
	}
	if (mClient) {
		disconnect(mClient, nullptr, this, nullptr);
	}
	mClient = client;
	if (mClient) {
		connect(mClient, &RedmineClient::currentUserReceived, this, &ProjectAccess::setCurrentUser);
		connect(mClient, &RedmineClient::rolesReceived, this, &ProjectAccess::setGivableRoles);
		connect(mClient, &RedmineClient::roleReceived, this, &ProjectAccess::setRole);
		connect(mClient, &RedmineClient::permissionLookupFailed, this, &ProjectAccess::stopWaiting);
	}
	emit clientChanged();
}

QJsonArray ProjectAccess::projects() const { return mProjects; }

void ProjectAccess::setProjects(const QJsonArray& projects)
{
	if (mProjects == projects) {
		return;
	}
	mProjects = projects;
	emit projectsChanged();
	update();
}

QVariantList ProjectAccess::issueProjects() const { return mIssueProjects; }

bool ProjectAccess::userKnown() const { return mUserKnown; }

void ProjectAccess::refresh()
{
	if (!mClient) {
		return;
	}
	mGivableRolesPending = true;
	mClient->getCurrentUser();
	mClient->getRoles();
}

void ProjectAccess::setCurrentUser(const QJsonObject& user)
{
	mUser = user;
	mRolePermissions.clear();
	mPendingRoleIds.clear();
	if (!mUser["admin"].toBool()) {
		mPendingRoleIds.insert(NON_MEMBER_ROLE_ID);
		for (const QJsonValue& membership : mUser["memberships"].toArray()) {
			for (const QJsonValue& role : membership["roles"].toArray()) {
				mPendingRoleIds.insert(role["id"].toInt());
			}
		}
	}
	if (!mUserKnown) {
		mUserKnown = true;
		emit userKnownChanged();
	}
	if (mClient) {
		for (const int roleId : std::as_const(mPendingRoleIds)) {
			mClient->getRole(roleId);
		}
	}
	update();
}

void ProjectAccess::setGivableRoles(const QJsonArray& roles)
{
	mGivableRoleIds.clear();
	for (const QJsonValue& role : roles) {
		mGivableRoleIds.insert(role["id"].toInt());
	}
	mGivableRolesKnown = true;
	mGivableRolesPending = false;
	update();
}

void ProjectAccess::setRole(const QJsonObject& role)
{
	const int roleId = role["id"].toInt();
	if (!mPendingRoleIds.remove(roleId)) {
		return; // not asked for by the current user's lookup
	}
	QStringList permissions;
	for (const QJsonValue& permission : role["permissions"].toArray()) {
		permissions.append(permission.toString());
	}
	mRolePermissions.insert(roleId, permissions);
	update();
}

void ProjectAccess::stopWaiting()
{
	mPendingRoleIds.clear();
	mGivableRolesPending = false;
	update();
}

bool ProjectAccess::roleAllows(int roleId) const
{
	const auto permissions = mRolePermissions.constFind(roleId);
	return permissions == mRolePermissions.cend() || permissions->contains(QStringLiteral("add_issues"));
}

bool ProjectAccess::canAddIssues(const QJsonObject& project) const
{
	if (project.contains("enabled_modules")) {
		const QJsonArray modules = project["enabled_modules"].toArray();
		const bool tracksIssues = std::any_of(modules.begin(), modules.end(),
			[](const QJsonValue& module) { return module["name"].toString() == QStringLiteral("issue_tracking"); });
		if (!tracksIssues) {
			return false;
		}
	}
	if (!mUserKnown || mUser["admin"].toBool()) {
		return true;
	}

	const int projectId = project["id"].toInt();
	for (const QJsonValue& membership : mUser["memberships"].toArray()) {
		if (membership["project"]["id"].toInt() != projectId) {
			continue;
		}
		const QJsonArray roles = membership["roles"].toArray();
		return std::any_of(
			roles.begin(), roles.end(), [this](const QJsonValue& role) { return roleAllows(role["id"].toInt()); });
	}

	if (!project["is_public"].toBool()) {
		return false;
	}
	// Id 1 is only known to be "Non member" once it's confirmed not to be a role given to members.
	if (!mGivableRolesKnown || mGivableRoleIds.contains(NON_MEMBER_ROLE_ID)) {
		return true;
	}
	return roleAllows(NON_MEMBER_ROLE_ID);
}

void ProjectAccess::update()
{
	if (!mPendingRoleIds.isEmpty() || mGivableRolesPending) {
		return;
	}
	QVariantList issueProjects;
	for (const QJsonValue& value : std::as_const(mProjects)) {
		const QJsonObject project = value.toObject();
		if (project["status"].toInt() == 1 && canAddIssues(project)) {
			issueProjects.append(project.toVariantMap());
		}
	}
	if (issueProjects == mIssueProjects) {
		return;
	}
	mIssueProjects = issueProjects;
	qCDebug(lcProjectAccess) << mIssueProjects.size() << "of" << mProjects.size() << "projects take new issues";
	emit issueProjectsChanged();
}
}
