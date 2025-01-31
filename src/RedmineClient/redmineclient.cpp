#include "redmineclient.h"
#include <QLoggingCategory>
#include <QUrlQuery>

#include "qsettingshelper.h"

Q_LOGGING_CATEGORY(lcRedmineClient, "qredmine.client")

namespace QRedmineClient {
RedmineClient::RedmineClient(QObject* parent)
	: QObject(parent)
	, mLoading(false)
	, mNetworkManager(new QNetworkAccessManager(this))
{
	connect(mNetworkManager, &QNetworkAccessManager::finished, this, &RedmineClient::handleNetworkReply);
	auto baseUrl = Utils::GetSetting("client/baseUrl").toString();
	auto apiKey = Utils::GetSetting("client/apiKey").toString();
	if (!baseUrl.isEmpty()) {
		setBaseUrl(baseUrl);
	}
	if (!apiKey.isEmpty()) {
		setApiKey(apiKey);
	}
}

RedmineClient::~RedmineClient() { delete mNetworkManager; }

QString RedmineClient::baseUrl() const { return mBaseUrl; }

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

void RedmineClient::createTimeEntry(
	int issueId, double hours, const QString& comments, const QDateTime& spentOn, int activityId)
{
	setLoading(true);

	QJsonObject timeEntryData;
	QJsonObject timeEntry;
	timeEntry["issue_id"] = issueId;
	timeEntry["spent_on"] = spentOn.toString("yyyy-MM-dd");
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

void RedmineClient::getIssue(int issueId)
{
	setLoading(true);
	mNetworkManager->get(createRequest(ApiResources::ISSUE.arg(QString::number(issueId))));
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
		qDebug(lcRedmineClient) << QT_TR_NOOP(QStringLiteral("Error response data: ")) << response;
		return;
	}

	QByteArray data = reply->readAll();
	QJsonDocument doc = QJsonDocument::fromJson(data);
	QJsonObject response = doc.object();

	QString operation = reply->url().path();
	if (reply->operation() == QNetworkAccessManager::PostOperation) {
		// Create time entry response
		if (response.contains("time_entry")) {
			emit timeEntryCreated(response["time_entry"].toObject());
		}
	} else if (reply->operation() == QNetworkAccessManager::PutOperation) {
		// Update time entry response
		int timeEntryId = operation.split('/').at(2).split('.').at(0).toInt();
		emit timeEntryUpdated(timeEntryId);
	} else if (reply->operation() == QNetworkAccessManager::GetOperation) {
		// Get time entries response
		if (response.contains("time_entries")) {
			emit timeEntriesReceived(response["time_entries"].toArray());
		}
		if (response.contains("issue")) {
			QJsonObject issue = response["issue"].toObject();
			qInfo() << "Received " << issue;
			emit issueReceived(issue);
		}
	}
}
}
