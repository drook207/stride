
#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrlQuery>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <QtQmlIntegration/qqmlintegration.h>

#ifdef QT_REDMINECLIENT_BUILD
#define QT_REDMINECLIENT_EXPORT Q_DECL_EXPORT
#else
#define QT_REDMINECLIENT_EXPORT Q_DECL_IMPORT
#endif

namespace QRedmineClient {
/*!
 * \brief The RedmineClient class
 */

namespace ApiResources {
	static const QString ISSUE = QStringLiteral("issues/%0.json"); // arg[0] == id
	static const QString ISSUES = QStringLiteral("issues.json");
	static const QString PROJECT = QStringLiteral("projects/%0.json"); // arg[0] == id
	static const QString PROJECTS = QStringLiteral("projects.json");
	static const QString CUSTOM_FIELDS = QStringLiteral("custom_fields.json");
	static const QString CURRENT_USER = QStringLiteral("users/current.json");
	static const QString ROLES = QStringLiteral("roles.json");
	static const QString ROLE = QStringLiteral("roles/%0.json"); // arg[0] == id
	static const QString TIME_ENTRY = QStringLiteral("time_entries/%0.json"); // arg[0] == id
	static const QString TIME_ENTRIES = QStringLiteral("time_entries.json");
}

class QT_REDMINECLIENT_EXPORT RedmineClient : public QObject {
	Q_OBJECT
	QML_ELEMENT
	Q_DISABLE_COPY_MOVE(RedmineClient)
	Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
	Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
	Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
	Q_PROPERTY(ConnectionState connectionState READ connectionState NOTIFY connectionStateChanged FINAL)
	Q_PROPERTY(bool useSSL READ useSSL WRITE useSSL NOTIFY useSSLChanged FINAL)
	Q_PROPERTY(int apiVersion READ apiVersion NOTIFY apiVersionChanged FINAL)
public:
	// Redmine's REST API version, not its own value until a response from the server has been seen
	static constexpr int UNKNOWN_API_VERSION = -1;

	enum EnumerationTypes {
		TIME_ENTRY_ACTIVITIES,
		ISSUE_PRIORITIES,
		DOCUMENT_CATEGORIES

	};
	Q_ENUM(EnumerationTypes)

	enum ConnectionState {
		OFFLINE, // Either never connected to server or API key / url is wrong
		TEMPORARY_OFFLINE, // Connection was established but now Redmine server is not reachable anymore
		ONLINE // Connected and authorized
	};
	Q_ENUM(ConnectionState)

	static const QList<QNetworkReply::NetworkError> REASONS_FOR_CONNECTION_LOSS;

	/*!
	 * \brief installTranslations Loads this library's translation catalog (embedded under
	 * :/i18n/QRedmineClient_<lang>.qm, see i18n/README.md) matching \a locale and installs it on the
	 * running QCoreApplication, so that user-facing strings such as the errorOccurred() messages are
	 * localized. Must be called after the QCoreApplication has been created. Calling it again (e.g.
	 * to switch the language at runtime) replaces the catalog installed by the previous call.
	 * \param locale Locale to load the catalog for, the system locale by default
	 * \return true if a matching catalog was found and installed, false otherwise (strings then
	 * fall back to their English source text)
	 */
	static bool installTranslations(const QLocale& locale = QLocale::system());

	/*!
	 * \brief RedmineClient
	 * \param parent
	 */
	explicit RedmineClient(QObject* parent = nullptr);

	~RedmineClient();

	/*!
	 * \brief baseUrl
	 * \return
	 */
	QString baseUrl() const;

	/*!
	 * \brief setBaseUrl
	 * \param url
	 */
	void setBaseUrl(const QString& url);

	/*!
	 * \brief apiKey
	 * \return
	 */
	QString apiKey() const;

	/*!
	 * \brief setApiKey
	 * \param key
	 */
	void setApiKey(const QString& key);

	/*!
	 * \brief loading
	 * \return
	 */
	bool loading() const;

	RedmineClient::ConnectionState connectionState() const;

	bool useSSL() const { return mUseSSL; }

	/*!
	 * \brief apiVersion The Redmine REST API version last reported by the server via the
	 * "X-Redmine-API-Version" response header, or UNKNOWN_API_VERSION when not known yet.
	 * This is Redmine's API feature-level version, not the Redmine application version.
	 * \return
	 */
	int apiVersion() const;

	/*!
	 * \brief hasApiVersion Whether the server's API version is known to be at least minVersion.
	 * Use this to guard calls to endpoints/parameters that were only added in later API versions.
	 * Returns false while the API version is still unknown.
	 * \param minVersion
	 * \return
	 */
	Q_INVOKABLE bool hasApiVersion(int minVersion) const;

public slots:
	/*!
	 * \brief detectApiVersion Issues a lightweight request so the server's API version can be
	 * read from the response headers. apiVersionChanged is emitted once it becomes known.
	 */
	void detectApiVersion();

	// Create a new time entry
	/*!
	 * \brief createTimeEntry
	 * \param issueId
	 * \param hours
	 * \param comments
	 * \param spentOn
	 * \param activityId
	 * \param customFields Custom field values as [{"id": <field id>, "value": <value>}], see
	 * getTimeEntryCustomFields()
	 * \param requestTag Passed on unchanged with timeEntryCreated(), e.g. to tell which local entry
	 * the time entry was made from
	 */
	void createTimeEntry(int issueId, double hours, const QString& comments,
		const QString& spentOn = QDateTime::currentDateTime().toString("yyyy-MM-dd"),
		int activityId = 4, // 4 is often "Development"
		const QVariantList& customFields = QVariantList(), int requestTag = 0);

	// Update an existing time entry
	/*!
	 * \brief updateTimeEntry Changes an existing time entry; emits timeEntryUpdated() on success
	 * \param timeEntryId Redmine's id of the time entry
	 * \param hours
	 * \param comments
	 * \param spentOn New date as "yyyy-MM-dd", left unchanged if empty
	 * \param activityId New activity, left unchanged if not positive
	 * \param customFields Custom field values to change, as for createTimeEntry(); fields not
	 * listed are left unchanged
	 * \param requestTag Passed on unchanged with timeEntryUpdated() or timeEntryUpdateFailed(), to
	 * tell which of several updates of the same time entry was confirmed or rejected
	 */
	void updateTimeEntry(int timeEntryId, double hours, const QString& comments, const QString& spentOn = QString(),
		int activityId = 0, const QVariantList& customFields = QVariantList(), int requestTag = 0);

	// Get time entries for an issue
	/*!
	 * \brief getTimeEntries
	 * \param issueId
	 */
	void getTimeEntries(int issueId);

	/*!
	 * \brief deleteTimeEntry
	 * \param timeEntryId
	 */
	void deleteTimeEntry(int timeEntryId);

	/*!
	 * \brief getEnumerations
	 */
	void getEnumerations(EnumerationTypes enumerationType);

	/*!
	 * \brief getTimeEntryCustomFields Fetches the custom fields of time entries, emitting
	 * timeEntryCustomFieldsReceived()
	 *
	 * Redmine only lists custom fields with their possible values to admins (GET
	 * /custom_fields.json). For everyone else, this falls back to the fields and values found on
	 * the latest time entries the user can see: values nobody has used yet are missing then, and
	 * each field's format is guessed (a field holding only "0"/"1" is taken as a boolean). Neither
	 * request reports errors via errorOccurred().
	 */
	void getTimeEntryCustomFields();

	/*!
	 * \brief getIssue
	 * \param issueId
	 * \param quiet Don't report failures via errorOccurred(), for fetches the user didn't ask for
	 */
	void getIssue(int issueId, bool quiet = false);

	/*!
	 * \brief getIssues List issues, optionally filtered by project or status
	 * \param projectId Only return issues of this project when > 0
	 * \param statusId Only return issues with this status when > 0
	 * \param limit Maximum number of issues to return
	 * \param offset Skip this many issues (for pagination)
	 */
	void getIssues(int projectId = 0, int statusId = 0, int limit = 100, int offset = 0);

	/*!
	 * \brief createIssue
	 * \param projectId
	 * \param subject
	 * \param description
	 * \param trackerId Redmine tracker id, ignored when <= 0
	 * \param priorityId Redmine priority id, ignored when <= 0
	 */
	void createIssue(int projectId, const QString& subject, const QString& description = QString(), int trackerId = 0,
		int priorityId = 0);

	/*!
	 * \brief updateIssue Partially updates an issue, empty/zero arguments are left untouched
	 * \param issueId
	 * \param subject
	 * \param description
	 * \param statusId
	 */
	void updateIssue(
		int issueId, const QString& subject = QString(), const QString& description = QString(), int statusId = 0);

	/*!
	 * \brief deleteIssue
	 * \param issueId
	 */
	void deleteIssue(int issueId);

	/*!
	 * \brief getProject
	 * \param projectId
	 */
	void getProject(int projectId);

	/*!
	 * \brief getProjects List projects, including the time entry activities each one allows
	 * \param limit Maximum number of projects to return
	 * \param offset Skip this many projects (for pagination)
	 */
	void getProjects(int limit = 100, int offset = 0);

	/*!
	 * \brief getAllProjects Lists all projects, requesting page after page; emits projectsReceived()
	 * once with all of them. A new call supersedes one still in progress.
	 */
	void getAllProjects();

	/*!
	 * \brief getCurrentUser Fetches the user the API key belongs to, with their project memberships
	 * and roles; emits currentUserReceived(). Starts a new permission lookup: replies to earlier
	 * getCurrentUser(), getRoles() and getRole() calls are ignored from then on. Failures aren't
	 * reported via errorOccurred().
	 */
	void getCurrentUser();

	/*!
	 * \brief getRoles Lists the roles that can be given to members (not the built-in "Non member"
	 * and "Anonymous" roles); emits rolesReceived(). Failures aren't reported via errorOccurred().
	 */
	void getRoles();

	/*!
	 * \brief getRole Fetches a role with its permissions; emits roleReceived(). Failures aren't
	 * reported via errorOccurred().
	 */
	void getRole(int roleId);

	/*!
	 * \brief getOwnTimeEntries Fetches all time entries of the user the API key belongs to within
	 * a range of days, page after page; emits ownTimeEntriesReceived() once with all of them.
	 * Failures aren't reported via errorOccurred(). A new call supersedes one still in progress.
	 * \param from First day, "yyyy-MM-dd"
	 * \param to Last day, "yyyy-MM-dd"
	 */
	void getOwnTimeEntries(const QString& from, const QString& to);

	/*!
	 * \brief createProject
	 * \param name
	 * \param identifier
	 * \param description
	 */
	void createProject(const QString& name, const QString& identifier, const QString& description = QString());

	/*!
	 * \brief updateProject Partially updates a project, empty arguments are left untouched
	 * \param projectId
	 * \param name
	 * \param description
	 */
	void updateProject(int projectId, const QString& name = QString(), const QString& description = QString());

	/*!
	 * \brief deleteProject
	 * \param projectId
	 */
	void deleteProject(int projectId);

	void useSSL(bool useSSL);
signals:
	/*!
	 * \brief baseUrlChanged
	 */
	void baseUrlChanged();

	/*!
	 * \brief apiKeyChanged
	 */
	void apiKeyChanged();

	/*!
	 * \brief loadingChanged
	 */
	void loadingChanged();

	/*!
	 * \brief timeEntryCreated
	 * \param timeEntry
	 * \param requestTag As passed to createTimeEntry()
	 */
	void timeEntryCreated(const QJsonObject& timeEntry, int requestTag);

	/*!
	 * \brief timeEntryUpdated
	 * \param timeEntryId
	 * \param requestTag As passed to updateTimeEntry()
	 */
	void timeEntryUpdated(int timeEntryId, int requestTag);

	/*!
	 * \brief timeEntryUpdateFailed Emitted when Redmine rejects an update of a time entry, or it
	 * can't be reached; errorOccurred() reports the reason
	 * \param timeEntryId
	 * \param requestTag As passed to updateTimeEntry()
	 */
	void timeEntryUpdateFailed(int timeEntryId, int requestTag);

	/*!
	 * \brief timeEntriesReceived
	 * \param entries
	 */
	void timeEntriesReceived(const QJsonArray& entries);

	/*!
	 * \brief timeEntryDeleted
	 * \param timeEntryId
	 */
	void timeEntryDeleted(int timeEntryId);

	/*!
	 * \brief enumerationsReceived
	 * \param entries
	 */
	void enumerationsReceived(const QJsonArray& enums);

	/*!
	 * \brief timeEntryActivitiesReceived
	 * \param enums
	 */
	void timeEntryActivitiesReceived(const QJsonArray& activities);

	/*!
	 * \brief timeEntryCustomFieldsReceived Result of getTimeEntryCustomFields()
	 * \param customFields Fields as listed by Redmine's /custom_fields.json:
	 * [{"id", "name", "field_format", "possible_values": [{"value", "label"}], ...}]
	 */
	void timeEntryCustomFieldsReceived(const QJsonArray& customFields);

	/*!
	 * \brief errorOccurred Emitted when a request fails (except for 404 Not Found)
	 * \param error Human-readable, translated description of the failure, suitable for showing to
	 * the user (see installTranslations())
	 */
	void errorOccurred(const QString& error);

	/*!
	 * \brief issueReceived Emitted every time a signal is received
	 * \param issue The QJsonObject containing issue details
	 */
	void issueReceived(const QJsonObject& issue);

	/*!
	 * \brief issuesReceived
	 * \param issues
	 */
	void issuesReceived(const QJsonArray& issues);

	/*!
	 * \brief issueCreated
	 * \param issue
	 */
	void issueCreated(const QJsonObject& issue);

	/*!
	 * \brief issueUpdated
	 * \param issueId
	 */
	void issueUpdated(int issueId);

	/*!
	 * \brief issueDeleted
	 * \param issueId
	 */
	void issueDeleted(int issueId);

	/*!
	 * \brief projectReceived
	 * \param project
	 */
	void projectReceived(const QJsonObject& project);

	/*!
	 * \brief projectsReceived
	 * \param projects
	 */
	void projectsReceived(const QJsonArray& projects);

	/*!
	 * \brief currentUserReceived
	 * \param user As returned by /users/current.json, including "memberships"
	 */
	void currentUserReceived(const QJsonObject& user);

	/*!
	 * \brief roleReceived
	 * \param role As returned by /roles/:id.json, including "permissions"
	 */
	void roleReceived(const QJsonObject& role);

	/*!
	 * \brief rolesReceived
	 * \param roles As returned by /roles.json ({id, name})
	 */
	void rolesReceived(const QJsonArray& roles);

	/*!
	 * \brief permissionLookupFailed Emitted when a getCurrentUser(), getRoles() or getRole() request
	 * of the current permission lookup fails (these failures aren't reported via errorOccurred())
	 */
	void permissionLookupFailed();

	/*!
	 * \brief ownTimeEntriesReceived
	 * \param timeEntries All of the user's time entries asked for by getOwnTimeEntries()
	 */
	void ownTimeEntriesReceived(const QJsonArray& timeEntries);

	/*!
	 * \brief projectCreated
	 * \param project
	 */
	void projectCreated(const QJsonObject& project);

	/*!
	 * \brief projectUpdated
	 * \param projectId
	 */
	void projectUpdated(int projectId);

	/*!
	 * \brief projectDeleted
	 * \param projectId
	 */
	void projectDeleted(int projectId);

	void connectionStateChanged();
	void useSSLChanged();

	/*!
	 * \brief apiVersionChanged Emitted once the server's API version becomes known or changes
	 */
	void apiVersionChanged();

private slots:
	/*!
	 * \brief handleNetworkReply
	 * \param reply
	 */
	void handleNetworkReply(QNetworkReply* reply);

	void activatePeriodicConnectionCheck();

	void checkConnection();

private:
	//! Marks requests whose replies are handled separately from handleNetworkReply()'s generic
	//! routing (stored in a request attribute, see ROUTING_ATTRIBUTE).
	enum RequestTag {
		NoTag,
		TimeEntryCustomFieldsTag,
		TimeEntryCustomFieldsFallbackTag,
		AllProjectsTag,
		OwnTimeEntriesTag,
		PermissionsTag
	};

	//! A list fetched page after page (getAllProjects(), getOwnTimeEntries()).
	struct PagedFetch {
		QString resource;
		QUrlQuery query; // without limit/offset
		QString key; // the list's key in each page's JSON
		bool quiet = false; // failures aren't reported via errorOccurred()
		int generation = 0; // counts calls, so pages of a superseded call are ignored
		QJsonArray collected; // items received so far
	};

	QHash<RequestTag, PagedFetch> mPagedFetches;
	//! Starts fetching a list, superseding a fetch with the same tag still in progress.
	void startPagedFetch(
		RequestTag tag, const QString& resource, const QUrlQuery& query, const QString& key, bool quiet);
	//! Requests the page starting at offset.
	void requestPage(RequestTag tag, int offset);
	//! Adds a received page, then requests the next one or emits all items.
	void handlePagedReply(QNetworkReply* reply, RequestTag tag);

	//! Counts getCurrentUser() calls, so replies for a superseded permission lookup are ignored.
	int mPermissionsGeneration = 0;
	void requestPermissionData(QNetworkRequest request);
	void handlePermissionsReply(QNetworkReply* reply);

	void handleTimeEntryCustomFieldsReply(QNetworkReply* reply, RequestTag tag);
	//! Counts getTimeEntryCustomFields() calls, so replies of a superseded call are ignored.
	int mCustomFieldsGeneration = 0;

	//! Sets the connection state a reply shows: ONLINE on success, OFFLINE on connection loss.
	void updateConnectionStateFromReply(QNetworkReply* reply);

	/*!
	 * \brief setLoading
	 * \param loading
	 */
	void setLoading(bool loading);

	ConnectionState mConnectionState = ConnectionState::OFFLINE;
	QTimer mConnectionCheckTimer;

	void setupConnectionCheckTimer();
	void setConnectionState(RedmineClient::ConnectionState newConnectionState);
	void toggleConnectionState(RedmineClient::ConnectionState newConnectionState);

	/*!
	 * \brief createRequest
	 * \param endpoint - Target resource, e.g issues.json, projects.json
	 * \param query - query and parameters for request
	 * \return
	 */
	QNetworkRequest createRequest(const QString& endpoint, const QUrlQuery& query = QUrlQuery());

	/*!
	 * \brief userErrorMessage Builds the translated, user-facing message emitted via errorOccurred()
	 * \param reply The failed reply
	 * \param response The parsed JSON body of the reply; Redmine reports validation failures (422)
	 * as an "errors" string array in it
	 * \return
	 */
	QString userErrorMessage(QNetworkReply* reply, const QJsonObject& response) const;

	/*!
	 * \brief updateApiVersionFromReply Reads the "X-Redmine-API-Version" header, if present,
	 * and emits apiVersionChanged when it differs from the previously known value.
	 * \param reply
	 */
	void updateApiVersionFromReply(QNetworkReply* reply);

	QString mBaseUrl;
	QString mApiKey;
	bool mLoading;
	QNetworkAccessManager* mNetworkManager;
	bool mUseSSL { true };
	int mApiVersion { UNKNOWN_API_VERSION };
};
}
