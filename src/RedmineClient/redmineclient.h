
#pragma once

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
	 * localized. Must be called after the QCoreApplication has been created.
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
	 */
	void createTimeEntry(int issueId, double hours, const QString& comments,
		const QString& spentOn = QDateTime::currentDateTime().toString("yyyy-MM-dd"),
		int activityId = 4); // 4 is often "Development"

	// Update an existing time entry
	/*!
	 * \brief updateTimeEntry
	 * \param timeEntryId
	 * \param hours
	 * \param comments
	 */
	void updateTimeEntry(int timeEntryId, double hours, const QString& comments);

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
	 * \brief getCustomFields
	 */
	void getCustomFields();

	/*!
	 * \brief getIssue
	 * \param issueId
	 */
	void getIssue(int issueId);

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
	 * \brief getProjects List projects
	 * \param limit Maximum number of projects to return
	 * \param offset Skip this many projects (for pagination)
	 */
	void getProjects(int limit = 100, int offset = 0);

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
	 */
	void timeEntryCreated(const QJsonObject& timeEntry);

	/*!
	 * \brief timeEntryUpdated
	 * \param timeEntryId
	 */
	void timeEntryUpdated(int timeEntryId);

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

	void customFieldsReceived(const QJsonArray& customFields);

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
