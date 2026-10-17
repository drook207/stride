#pragma once

#include "redmineclient.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace QRedmineClient {

/*!
 * \brief Which projects the user may add issues to.
 *
 * Takes all projects (RedmineClient::getAllProjects()) and, via client, the user's memberships and
 * the permissions of their roles, and offers the active projects with issue tracking the user may
 * add issues to: by the roles of their membership in a project, or by the built-in "Non member"
 * role in public projects they aren't a member of. Admins may add issues anywhere.
 *
 * Whatever isn't known (yet, or because fetching it failed) counts as allowed, so projects are
 * offered rather than hidden. The result is published once all requested roles have arrived, not
 * once per role. Redmine can also restrict add_issues to some trackers, which its API doesn't
 * expose; such projects are still offered.
 */
class QT_REDMINECLIENT_EXPORT ProjectAccess : public QObject {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(RedmineClient* client READ client WRITE setClient NOTIFY clientChanged FINAL)
	Q_PROPERTY(QJsonArray projects READ projects WRITE setProjects NOTIFY projectsChanged FINAL)
	Q_PROPERTY(QVariantList issueProjects READ issueProjects NOTIFY issueProjectsChanged FINAL)
	Q_PROPERTY(bool userKnown READ userKnown NOTIFY userKnownChanged FINAL)
public:
	//! Redmine's built-in "Non member" role is created by its installation before any other role, so
	//! it normally has id 1. Only trusted while getRoles() confirms id 1 isn't a role given to
	//! members (built-in roles aren't listed there).
	static constexpr int NON_MEMBER_ROLE_ID = 1;

	explicit ProjectAccess(QObject* parent = nullptr);

	RedmineClient* client() const;
	void setClient(RedmineClient* client);

	//! All projects, as received from RedmineClient::projectsReceived.
	QJsonArray projects() const;
	void setProjects(const QJsonArray& projects);

	//! The active projects with issue tracking the user may add issues to.
	QVariantList issueProjects() const;

	//! Whether the user's memberships are known (see refresh()).
	bool userKnown() const;

	//! Whether the user may add issues to project, as far as known.
	bool canAddIssues(const QJsonObject& project) const;

	//! Fetches the user's memberships and the roles again through client, e.g. after the API key
	//! changed. The current result stays until the new one is complete.
	Q_INVOKABLE void refresh();

public slots:
	//! Takes the user (with "memberships") and requests the permissions of their roles.
	void setCurrentUser(const QJsonObject& user);
	//! Takes the roles that can be given to members (RedmineClient::rolesReceived).
	void setGivableRoles(const QJsonArray& roles);
	//! Takes a role with its "permissions" (RedmineClient::roleReceived).
	void setRole(const QJsonObject& role);
	//! Stops waiting for permissions that failed to arrive and publishes with what's known
	//! (RedmineClient::permissionLookupFailed).
	void stopWaiting();

signals:
	void clientChanged();
	void projectsChanged();
	void issueProjectsChanged();
	void userKnownChanged();

private:
	//! Whether the role allows adding issues; unknown roles do.
	bool roleAllows(int roleId) const;
	//! Recomputes issueProjects, unless permissions requested for the current user are still missing.
	void update();

	QPointer<RedmineClient> mClient;
	QJsonArray mProjects;
	QVariantList mIssueProjects;
	QJsonObject mUser;
	bool mUserKnown = false;
	QSet<int> mGivableRoleIds;
	bool mGivableRolesKnown = false;
	QHash<int, QStringList> mRolePermissions;
	//! Roles requested for the current user that haven't arrived yet.
	QSet<int> mPendingRoleIds;
	bool mGivableRolesPending = false;
};
}
