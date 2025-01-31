#include "redmineclient.h"
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QStringLiteral>
#include <QUrlQuery>
#include <QtLogging>

#include "qsettingshelper.h"

Q_LOGGING_CATEGORY(lcRedmineClient, "qredmine.client")

namespace QRedmineClient {

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
		qInfo() << QStringLiteral("Connection state changed to: %0").arg(state);
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
	auto key = Utils::GetSetting("client/apiKey").toString();
	if (!url.isEmpty()) {
		setBaseUrl(url);
	}
	if (!key.isEmpty()) {
		setApiKey(key);
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
	if (mBaseUrl != url) {
		mBaseUrl = url;
		Utils::SaveSetting("client/baseUrl", mBaseUrl);
		emit baseUrlChanged();
	}
}

QString RedmineClient::apiKey() const { return mApiKey; }

void RedmineClient::setApiKey(const QString& key)
{
	if (mApiKey != key) {
		mApiKey = key;
		Utils::SaveSetting("client/apiKey", mApiKey);
		Utils::SaveSetting("client/apiKey", mApiKey);
		emit apiKeyChanged();
	}
}

bool RedmineClient::loading() const { return mLoading; }

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
	// FIXME Currently not supported
	// TODO Add fields to UI
	// timeEntry["activity_id"] = activityId;

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

void RedmineClient::getEnumerations(EnumerationTypes enumerationType)
{
	setLoading(true);
	QString endpoint;
	switch (enumerationType) {
		case TIME_ENTRY_ACTIVITIES:
			endpoint = QString("/enumerations/time_entry_activities.json");
			break;
		case ISSUE_PRIORITIES:
			endpoint = QString("/enumerations/issue_priorities.json");
			break;
		case DOCUMENT_CATEGORIES:
			endpoint = QString("/enumerations/document_categories.json");
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
	QString endpoint = QString("/custom_fields.json");
	mNetworkManager->get(createRequest(endpoint));
}

void RedmineClient::handleNetworkReply(QNetworkReply* reply)
{
	reply->deleteLater();
	setLoading(false);

	if (reply->error() != QNetworkReply::NoError) {
		emit errorOccurred(reply->errorString());
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
			break;
		case QNetworkAccessManager::PutOperation:
			path = reply->url().path();
			// TODO Add logic to ensure that the recepted id is the correct one
			// TODO Make the split more obvious
			id = path.split('/').at(2).split('.').at(0).toInt();
			emit timeEntryUpdated(id);
			break;

		case QNetworkAccessManager::PostOperation:
			if (response.contains("time_entry")) {
				emit timeEntryCreated(response["time_entry"].toObject());
			}
			break;
		case QNetworkAccessManager::DeleteOperation:
			break;
		case QNetworkAccessManager::CustomOperation:
			break;
		case QNetworkAccessManager::UnknownOperation:
			break;
	}
}
}
