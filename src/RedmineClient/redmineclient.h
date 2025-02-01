
#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
	Q_PLUGIN_METADATA(IID "drook207.redmineClient")
	Q_DISABLE_COPY_MOVE(RedmineClient)
	Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
	Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
	Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
	Q_PROPERTY(ConnectionState connectionState READ connectionState NOTIFY connectionStateChanged FINAL)
public:
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
public slots:

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
	 * \brief getEnumerations
	 */
	void getEnumerations(EnumerationTypes enumerationType);

	/*!
	 * \brief getCustomFields
	 */
	void getCustomFields();

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
	 * \brief errorOccurred
	 * \param error
	 */
	void errorOccurred(const QString& error);

	void connectionStateChanged();

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
	QString mBaseUrl;
	QString mApiKey;
	bool mLoading;
	QNetworkAccessManager* mNetworkManager;
};
}
Q_DECLARE_METATYPE(QRedmineClient::RedmineClient)
