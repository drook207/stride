#include "redmineclient.h"
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QStringLiteral>
#include <QUrlQuery>
#include <QtLogging>

#include "qsettingshelper.h"
#include "securestorage.h"

Q_LOGGING_CATEGORY(lcRedmineClient, "qredmine.client")

namespace QRedmineClient {

// Key the API key is stored under within keychainService(), and the legacy QSettings key it used to
// be (insecurely) persisted under before it was migrated to the OS-native secure storage.
static const QString API_KEY_KEYCHAIN_KEY = QStringLiteral("apiKey");
static const QString LEGACY_API_KEY_SETTING = QStringLiteral("client/apiKey");

// Service identifier the API key is stored under in the OS-native secure storage (see securestorage.h).
// Derived from the application name rather than a fixed literal so that, e.g., the test binary
// (which sets its own QCoreApplication::applicationName(), see tests/main.cpp) never reads from or
// writes to the same keychain entry as the real application.
static QString keychainService()
{
	const QString appName = QCoreApplication::applicationName();
	return appName.isEmpty() ? QStringLiteral("QRedmineTimeTracker") : appName;
}

const QList<QNetworkReply::NetworkError> RedmineClient::REASONS_FOR_CONNECTION_LOSS = {
	QNetworkReply::HostNotFoundError, QNetworkReply::TimeoutError, QNetworkReply::OperationCanceledError,
	QNetworkReply::TemporaryNetworkFailureError, QNetworkReply::AuthenticationRequiredError,
	QNetworkReply::UnknownServerError
};

void QRedmineClient::RedmineClient::setupConnectionCheckTimer()
{
	mConnectionCheckTimer.setInterval(5000);
	connect(&mConnectionCheckTimer, &QTimer::timeout, this, &RedmineClient::checkConnection);

	connect(this, &RedmineClient::baseUrlChanged, this, &RedmineClient::activatePeriodicConnectionCheck);
	connect(this, &RedmineClient::apiKeyChanged, this, &RedmineClient::activatePeriodicConnectionCheck);
	connect(this, &RedmineClient::connectionStateChanged, this, [&] {
		QString state = QMetaEnum::fromType<RedmineClient::ConnectionState>().valueToKey(mConnectionState);
		qDebug(lcRedmineClient) << QStringLiteral("Connection state changed to: %0").arg(state);
	});
	activatePeriodicConnectionCheck();
}

RedmineClient::RedmineClient(QObject* parent)
	: QObject(parent)
	, mLoading(false)
	, mNetworkManager(new QNetworkAccessManager(this))
{
	connect(mNetworkManager, &QNetworkAccessManager::finished, this, &RedmineClient::handleNetworkReply);
	auto url = Utils::GetSetting("client/baseUrl").toString();
	if (!url.isEmpty()) {
		setBaseUrl(url);
	}

	// Migrate a legacy plaintext API key out of QSettings into the OS-native secure storage, if one
	// is still present from before this migration existed. setApiKey() already takes care of writing
	// the new value to the keychain and updating mApiKey/emitting apiKeyChanged synchronously.
	auto legacyKey = Utils::GetSetting(LEGACY_API_KEY_SETTING).toString();
	if (!legacyKey.isEmpty()) {
		setApiKey(legacyKey);
		Utils::RemoveSetting(LEGACY_API_KEY_SETTING);
	} else {
		// Nothing to migrate, load the API key from the keychain asynchronously instead. Applying it
		// here must not trigger another write back to the keychain.
		Utils::ReadSecret(
			keychainService(), API_KEY_KEYCHAIN_KEY,
			[this](const QString& key) {
				if (mApiKey != key) {
					mApiKey = key;
					emit apiKeyChanged();
				}
			},
			{}, {}, this);
	}

	setupConnectionCheckTimer();
}

RedmineClient::~RedmineClient() { delete mNetworkManager; }

QString RedmineClient::baseUrl() const { return mBaseUrl; }

void RedmineClient::activatePeriodicConnectionCheck()
{
	if (mBaseUrl.isEmpty() || mApiKey.isEmpty()) {
		return;
	}

	mConnectionCheckTimer.start();
}

void RedmineClient::checkConnection()
{
	QUrlQuery query;
	query.addQueryItem("limit", 0);
	mNetworkManager->get(createRequest(ApiResources::ISSUES, query));
}

void RedmineClient::setBaseUrl(const QString& url)
{
	// createRequest() builds request URLs by concatenating mBaseUrl directly with endpoints such as
	// "issues.json", so mBaseUrl must always end with a slash or every request URL would be malformed.
	QString normalizedUrl = url;
	if (!normalizedUrl.isEmpty() && !normalizedUrl.endsWith('/')) {
		normalizedUrl += '/';
	}

	if (mBaseUrl != normalizedUrl) {
		mBaseUrl = normalizedUrl;
		Utils::SaveSetting("client/baseUrl", mBaseUrl);
		emit baseUrlChanged();
	}
}

QString RedmineClient::apiKey() const { return mApiKey; }

void RedmineClient::setApiKey(const QString& key)
{
	if (mApiKey != key) {
		mApiKey = key;
		Utils::WriteSecret(keychainService(), API_KEY_KEYCHAIN_KEY, mApiKey);
		emit apiKeyChanged();
	}
}

void RedmineClient::useSSL(bool useSSL)
{
	if (mUseSSL != useSSL) {
		mUseSSL = useSSL;
		mNetworkManager->setStrictTransportSecurityEnabled(useSSL);
		emit useSSLChanged();
	}
}

bool RedmineClient::loading() const { return mLoading; }

int RedmineClient::apiVersion() const { return mApiVersion; }

bool RedmineClient::hasApiVersion(int minVersion) const
{
	return mApiVersion != UNKNOWN_API_VERSION && mApiVersion >= minVersion;
}

void RedmineClient::detectApiVersion()
{
	setLoading(true);
	QUrlQuery query;
	query.addQueryItem("limit", "0");
	mNetworkManager->get(createRequest(ApiResources::ISSUES, query));
}

void RedmineClient::updateApiVersionFromReply(QNetworkReply* reply)
{
	if (!reply->hasRawHeader("X-Redmine-API-Version")) {
		return;
	}

	bool ok = false;
	int version = reply->rawHeader("X-Redmine-API-Version").toInt(&ok);
	if (ok && version != mApiVersion) {
		mApiVersion = version;
		emit apiVersionChanged();
	}
}

void RedmineClient::setLoading(bool loading)
{
	if (mLoading != loading) {
		mLoading = loading;
		emit loadingChanged();
	}
}

QNetworkRequest RedmineClient::createRequest(const QString& endpoint, const QUrlQuery& query)
{
	QUrl url(mBaseUrl + endpoint + "?" + query.toString());
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("X-Redmine-API-Key", mApiKey.toUtf8());
	return request;
}

RedmineClient::ConnectionState RedmineClient::connectionState() const { return mConnectionState; }

void RedmineClient::setConnectionState(RedmineClient::ConnectionState newConnectionState)
{
	if (mConnectionState == newConnectionState) {
		return;
	}
	mConnectionState = newConnectionState;
	emit connectionStateChanged();
}

void RedmineClient::toggleConnectionState(ConnectionState newConnectionState)
{
	ConnectionState target = newConnectionState;
	switch (mConnectionState) {
		case OFFLINE:
			target = newConnectionState;
			break;
		case TEMPORARY_OFFLINE:
		case ONLINE:
			// We have been online before, so we only become temporary offline again
			target = OFFLINE == newConnectionState ? TEMPORARY_OFFLINE : newConnectionState;
			break;
	}
	setConnectionState(target);
}

void RedmineClient::createTimeEntry(
	int issueId, double hours, const QString& comments, const QString& spentOn, int activityId)
{
	setLoading(true);

	QJsonObject timeEntryData;
	QJsonObject timeEntry;
	timeEntry["issue_id"] = issueId;
	timeEntry["spent_on"] = spentOn;
	timeEntry["hours"] = hours;
	timeEntry["comments"] = comments;
	if (activityId > 0) {
		timeEntry["activity_id"] = activityId;
	}

	// FIXME Currently not supported
	// TODO Add fields to UI
	//  timeEntry["user_id"] = 0;

	timeEntryData["time_entry"] = timeEntry;

	QJsonDocument doc(timeEntryData);
	QByteArray data = doc.toJson();

	mNetworkManager->post(createRequest(ApiResources::TIME_ENTRIES), data);
}

void RedmineClient::updateTimeEntry(int timeEntryId, double hours, const QString& comments)
{
	setLoading(true);

	QJsonObject timeEntryData;
	QJsonObject timeEntry;
	timeEntry["hours"] = hours;
	timeEntry["comments"] = comments;
	timeEntryData["time_entry"] = timeEntry;

	QJsonDocument doc(timeEntryData);
	QByteArray data = doc.toJson();

	QString endpoint = ApiResources::TIME_ENTRY.arg(timeEntryId);
	mNetworkManager->put(createRequest(endpoint), data);
}

void RedmineClient::getTimeEntries(int issueId)
{
	setLoading(true);
	QUrlQuery query;
	query.addQueryItem("issue_id", QString::number(issueId));
	mNetworkManager->get(createRequest(ApiResources::TIME_ENTRIES, query));
}

void RedmineClient::deleteTimeEntry(int timeEntryId)
{
	setLoading(true);
	mNetworkManager->deleteResource(createRequest(ApiResources::TIME_ENTRY.arg(timeEntryId)));
}

void RedmineClient::getIssue(int issueId)
{
	setLoading(true);
	mNetworkManager->get(createRequest(ApiResources::ISSUE.arg(QString::number(issueId))));
}

void RedmineClient::getIssues(int projectId, int statusId, int limit, int offset)
{
	setLoading(true);

	QUrlQuery query;
	if (projectId > 0) {
		query.addQueryItem("project_id", QString::number(projectId));
	}
	if (statusId > 0) {
		query.addQueryItem("status_id", QString::number(statusId));
	}
	query.addQueryItem("limit", QString::number(limit));
	query.addQueryItem("offset", QString::number(offset));

	mNetworkManager->get(createRequest(ApiResources::ISSUES, query));
}

void RedmineClient::createIssue(
	int projectId, const QString& subject, const QString& description, int trackerId, int priorityId)
{
	setLoading(true);

	QJsonObject issue;
	issue["project_id"] = projectId;
	issue["subject"] = subject;
	if (!description.isEmpty()) {
		issue["description"] = description;
	}
	if (trackerId > 0) {
		issue["tracker_id"] = trackerId;
	}
	if (priorityId > 0) {
		issue["priority_id"] = priorityId;
	}

	QJsonObject issueData;
	issueData["issue"] = issue;

	QJsonDocument doc(issueData);
	mNetworkManager->post(createRequest(ApiResources::ISSUES), doc.toJson());
}

void RedmineClient::updateIssue(int issueId, const QString& subject, const QString& description, int statusId)
{
	setLoading(true);

	QJsonObject issue;
	if (!subject.isEmpty()) {
		issue["subject"] = subject;
	}
	if (!description.isEmpty()) {
		issue["description"] = description;
	}
	if (statusId > 0) {
		issue["status_id"] = statusId;
	}

	QJsonObject issueData;
	issueData["issue"] = issue;

	QJsonDocument doc(issueData);
	QString endpoint = ApiResources::ISSUE.arg(issueId);
	mNetworkManager->put(createRequest(endpoint), doc.toJson());
}

void RedmineClient::deleteIssue(int issueId)
{
	setLoading(true);
	mNetworkManager->deleteResource(createRequest(ApiResources::ISSUE.arg(issueId)));
}

void RedmineClient::getProject(int projectId)
{
	setLoading(true);
	mNetworkManager->get(createRequest(ApiResources::PROJECT.arg(projectId)));
}

void RedmineClient::getProjects(int limit, int offset)
{
	setLoading(true);

	QUrlQuery query;
	query.addQueryItem("limit", QString::number(limit));
	query.addQueryItem("offset", QString::number(offset));

	mNetworkManager->get(createRequest(ApiResources::PROJECTS, query));
}

void RedmineClient::createProject(const QString& name, const QString& identifier, const QString& description)
{
	setLoading(true);

	QJsonObject project;
	project["name"] = name;
	project["identifier"] = identifier;
	if (!description.isEmpty()) {
		project["description"] = description;
	}

	QJsonObject projectData;
	projectData["project"] = project;

	QJsonDocument doc(projectData);
	mNetworkManager->post(createRequest(ApiResources::PROJECTS), doc.toJson());
}

void RedmineClient::updateProject(int projectId, const QString& name, const QString& description)
{
	setLoading(true);

	QJsonObject project;
	if (!name.isEmpty()) {
		project["name"] = name;
	}
	if (!description.isEmpty()) {
		project["description"] = description;
	}

	QJsonObject projectData;
	projectData["project"] = project;

	QJsonDocument doc(projectData);
	QString endpoint = ApiResources::PROJECT.arg(projectId);
	mNetworkManager->put(createRequest(endpoint), doc.toJson());
}

void RedmineClient::deleteProject(int projectId)
{
	setLoading(true);
	mNetworkManager->deleteResource(createRequest(ApiResources::PROJECT.arg(projectId)));
}

void RedmineClient::getEnumerations(EnumerationTypes enumerationType)
{
	setLoading(true);
	QString endpoint;
	switch (enumerationType) {
		case TIME_ENTRY_ACTIVITIES:
			endpoint = QString("enumerations/time_entry_activities.json");
			break;
		case ISSUE_PRIORITIES:
			endpoint = QString("enumerations/issue_priorities.json");
			break;
		case DOCUMENT_CATEGORIES:
			endpoint = QString("enumerations/document_categories.json");
			break;
		default:
			qWarning(lcRedmineClient()) << QT_TR_NOOP(
				QStringLiteral("No valid Enumeration type is provided: <%0>").arg(enumerationType));
			return;
	}

	mNetworkManager->get(createRequest(endpoint));
}

void RedmineClient::getCustomFields()
{
	setLoading(true);
	QString endpoint = QString("custom_fields.json");
	mNetworkManager->get(createRequest(endpoint));
}

void RedmineClient::handleNetworkReply(QNetworkReply* reply)
{
	reply->deleteLater();
	setLoading(false);
	updateApiVersionFromReply(reply);

	if (reply->error() != QNetworkReply::NoError) {
		if (reply->error() != QNetworkReply::ContentNotFoundError) {
			emit errorOccurred(reply->errorString());
		}
		QByteArray data = reply->readAll();
		QJsonDocument doc = QJsonDocument::fromJson(data);
		QJsonObject response = doc.object();

		qWarning(lcRedmineClient) << tr("[%0] %1: ").arg(reply->error()).arg(reply->errorString())
								  << QJsonDocument(response).toJson(QJsonDocument::Compact);

		if (REASONS_FOR_CONNECTION_LOSS.contains(reply->error())) {
			toggleConnectionState(ConnectionState::OFFLINE);
			if (reply->error() == QNetworkReply::HostNotFoundError ||
				reply->error() == QNetworkReply::AuthenticationRequiredError) {
				mConnectionCheckTimer.stop();
			}
		}

		return;
	}

	activatePeriodicConnectionCheck();
	setConnectionState(ConnectionState::ONLINE);

	QByteArray data = reply->readAll();
	QJsonDocument doc = QJsonDocument::fromJson(data);
	QJsonObject response = doc.object();

	QString path {};
	int id {};
	switch (reply->operation()) {
		case QNetworkAccessManager::HeadOperation:
			break;
		case QNetworkAccessManager::GetOperation:
			if (response.contains("time_entries")) {
				emit timeEntriesReceived(response["time_entries"].toArray());
			}
			if (response.contains("time_entry_activities")) {
				emit timeEntryActivitiesReceived(response["time_entry_activities"].toArray());
			}
			if (response.contains("issue_priorities")) {
				emit enumerationsReceived(response["issue_priorities"].toArray());
			}
			if (response.contains("document_categories")) {
				emit enumerationsReceived(response["document_categories"].toArray());
			}
			if (response.contains("custom_fields")) {
				emit customFieldsReceived(response["custom_fields"].toArray());
			}
			if (response.contains("issue")) {
				emit issueReceived(response["issue"].toObject());
			}
			if (response.contains("issues")) {
				emit issuesReceived(response["issues"].toArray());
			}
			if (response.contains("project")) {
				emit projectReceived(response["project"].toObject());
			}
			if (response.contains("projects")) {
				emit projectsReceived(response["projects"].toArray());
			}
			break;
		case QNetworkAccessManager::PutOperation:
			// TODO Add logic to ensure that the recepted id is the correct one
			// TODO Make the split more obvious
			path = reply->url().path();
			id = path.split('/').at(2).split('.').at(0).toInt();
			if (path.contains("/time_entries/")) {
				emit timeEntryUpdated(id);
			} else if (path.contains("/issues/")) {
				emit issueUpdated(id);
			} else if (path.contains("/projects/")) {
				emit projectUpdated(id);
			}
			break;

		case QNetworkAccessManager::PostOperation:
			if (response.contains("time_entry")) {
				emit timeEntryCreated(response["time_entry"].toObject());
			}
			if (response.contains("issue")) {
				emit issueCreated(response["issue"].toObject());
			}
			if (response.contains("project")) {
				emit projectCreated(response["project"].toObject());
			}
			break;
		case QNetworkAccessManager::DeleteOperation:
			path = reply->url().path();
			id = path.split('/').at(2).split('.').at(0).toInt();
			if (path.contains("/time_entries/")) {
				emit timeEntryDeleted(id);
			} else if (path.contains("/issues/")) {
				emit issueDeleted(id);
			} else if (path.contains("/projects/")) {
				emit projectDeleted(id);
			}
			break;
		case QNetworkAccessManager::CustomOperation:
			break;
		case QNetworkAccessManager::UnknownOperation:
			break;
	}
}
}
