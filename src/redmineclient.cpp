#include "redmineclient.h"
#include <QUrlQuery>

RedmineClient::RedmineClient(QObject *parent)
	: QObject(parent)
	, mLoading(false)
	, mNetworkManager(new QNetworkAccessManager(this))
{
	connect(mNetworkManager, &QNetworkAccessManager::finished,
			this, &RedmineClient::handleNetworkReply);
}

RedmineClient::~RedmineClient()
{
	delete mNetworkManager;
}

QString RedmineClient::baseUrl() const { return mBaseUrl; }

void RedmineClient::setBaseUrl(const QString &url)
{
	if (mBaseUrl != url) {
		mBaseUrl = url;
		emit baseUrlChanged();
	}
}

QString RedmineClient::apiKey() const { return mApiKey; }

void RedmineClient::setApiKey(const QString &key)
{
	if (mApiKey != key) {
		mApiKey = key;
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

QNetworkRequest RedmineClient::createRequest(const QString &endpoint)
{
	QUrl url(mBaseUrl + endpoint);
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("X-Redmine-API-Key", mApiKey.toUtf8());
	return request;
}

void RedmineClient::createTimeEntry(int issueId, double hours, const QString &comments,
								  const QDateTime &spentOn, int activityId)
{
	setLoading(true);

	QJsonObject timeEntryData;
	QJsonObject timeEntry;
	timeEntry["issue_id"] = issueId;
	timeEntry["hours"] = hours;
	timeEntry["activity_id"] = activityId;
	timeEntry["comments"] = comments;
	timeEntry["spent_on"] = spentOn.toString(Qt::ISODate);
	timeEntryData["time_entry"] = timeEntry;

	QJsonDocument doc(timeEntryData);
	QByteArray data = doc.toJson();

	mNetworkManager->post(createRequest("/time_entries.json"), data);
}

void RedmineClient::updateTimeEntry(int timeEntryId, double hours, const QString &comments)
{
	setLoading(true);

	QJsonObject timeEntryData;
	QJsonObject timeEntry;
	timeEntry["hours"] = hours;
	timeEntry["comments"] = comments;
	timeEntryData["time_entry"] = timeEntry;

	QJsonDocument doc(timeEntryData);
	QByteArray data = doc.toJson();

	QString endpoint = QString("/time_entries/%1.json").arg(timeEntryId);
	mNetworkManager->put(createRequest(endpoint), data);
}

void RedmineClient::getTimeEntries(int issueId)
{
	setLoading(true);
	QString endpoint = QString("/time_entries.json?issue_id=%1").arg(issueId);
	mNetworkManager->get(createRequest(endpoint));
}

void RedmineClient::handleNetworkReply(QNetworkReply *reply)
{
	reply->deleteLater();
	setLoading(false);

	if (reply->error() != QNetworkReply::NoError) {
		emit errorOccurred(reply->errorString());
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
	}
}
