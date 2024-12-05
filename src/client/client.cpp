#include "client.h"
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QRestAccessManager>
#include <memory>
#include <qbytearrayview.h>
#include <qcontainerfwd.h>
#include <qhttpheaders.h>
#include <qjsondocument.h>
#include <qnetworkaccessmanager.h>
#include <qnetworkreply.h>
#include <qvariant.h>
#include <utility>

namespace QRedmine {

Q_LOGGING_CATEGORY(lcClient, "qredmineclient.client")

RestClient::RestClient(QUrl url, const QStringView& apiKey, bool sslVerify, QObject* parent)
	: QObject(parent)
	, mUrl(std::move(url))
	, mSslVerify(sslVerify)
{
	if (mUrl.isValid()) {
		mNetworkManager = new QNetworkAccessManager(this);
		connect(mNetworkManager, &QNetworkAccessManager::sslErrors, this, &RestClient::handleSslError);
		mRestAccesManager = std::make_unique<QRestAccessManager>(mNetworkManager, this);
		mFactory.setBaseUrl(mUrl);
		mFactory.setBearerToken(apiKey.toLatin1());
		QHttpHeaders additionalHeaders {};
		bool result = true;
		result &= additionalHeaders.append(QHttpHeaders::WellKnownHeader::UserAgent, "redmine");
		result &= additionalHeaders.append("X-Custom-User-Agent", "redmine");
		result &= additionalHeaders.append(QHttpHeaders::WellKnownHeader::ContentType, "application/json");
		result &= additionalHeaders.append("X-Redmine-API-Keys", apiKey);

		if (result) {
			mFactory.setCommonHeaders(additionalHeaders);
			qDebug(lcClient()) << "Successfully set additional headers.";
		}
		return;
	}
	qWarning(lcClient) << "Invalid url, could not create client.";
}

QUrl RestClient::Url() const { return mUrl; }

void RestClient::setUrl(const QUrl& newUrl) { mUrl = newUrl; }

bool RestClient::SslVerify() const { return mSslVerify; }

void RestClient::setSslVerify(bool newSslVerify) { mSslVerify = newSslVerify; }

void RestClient::handleSslError(QNetworkReply* const reply, const QList<QSslError>& errors) const
{
	if (!mSslVerify) {
		reply->ignoreSslErrors();
		// return;
	}
	qWarning(lcClient()) << "SSL verification failed with the following errors.";
	for (const auto& error : std::as_const(errors)) {
		qWarning(lcClient()) << "Error: " << error.errorString();
	}
}

void RestClient::replyFinished() { qDebug(lcClient()) << "Reply finished!"; }

bool RestClient::getTimeEntrieFromIssueId(const QStringView& id, std::function<void(QRestReply&)> callback)
{
	if (id.isEmpty() || id.isNull()) {
		return false;
	}

	if (nullptr == callback) {
		return false;
	}

	auto reply = mRestAccesManager->get(
		mFactory.createRequest(QLatin1StringView("time_entries/" + id.toLatin1() + ".json")), this, callback);
	connect(reply, &QNetworkReply::finished, this, &RestClient::replyFinished);

	return true;
}

bool RestClient::getTimeEntryFromProjectId(const QStringView& id, std::function<void(QRestReply&)> callback)
{
	if (id.isEmpty() || id.isNull()) {
		return false;
	}

	if (nullptr == callback) {
		return false;
	}

	auto reply = mRestAccesManager->get(
		mFactory.createRequest(QLatin1StringView("time_entries.json?project_id=" + id.toLatin1())), this, callback);
	connect(reply, &QNetworkReply::finished, this, &RestClient::replyFinished);

	return true;
}

bool RestClient::getTimeEntries(std::function<void(QRestReply&)> callback)
{
	if (nullptr == callback) {
		return false;
	}

	auto reply = mRestAccesManager->get(mFactory.createRequest(QLatin1StringView("time_entries.json")), this, callback);
	connect(reply, &QNetworkReply::finished, this, &RestClient::replyFinished);

	return true;
}

bool RestClient::getTimeEntriesWithFilter(
	const QStringView& id, const QDate& from, const QDate& to, std::function<void(QRestReply&)> callback, quint8 limit)
{
	if (id.isEmpty() || id.isNull()) {
		return false;
	}

	if (!from.isValid() || from.isNull()) {
		return false;
	}

	if (!to.isValid() || to.isNull()) {
		return false;
	}

	if (limit > 100) {
		return false;
	}

	if (nullptr == callback) {
		return false;
	}

	auto requestStringView = QString("time_entries.json?project_id=%0&from=%1&to=%2&limit=%3")
								 .arg(id)
								 .arg(from.toString())
								 .arg(to.toString())
								 .arg(limit);

	auto reply = mRestAccesManager->get(mFactory.createRequest(requestStringView), this, callback);
	connect(reply, &QNetworkReply::finished, this, &RestClient::replyFinished);

	return true;
}

bool RestClient::createTimeEntry(const Types::TimeEntry& entry, std::function<void(QRestReply&)> callback)
{
	if (nullptr == callback) {
		return false;
	}
	// FIXME Create to and from Json Method for Types::TimeEntry
	const auto json = QJsonDocument();
	auto reply = mRestAccesManager->post(mFactory.createRequest(), json, this, callback);
	connect(reply, &QNetworkReply::finished, this, &RestClient::replyFinished);

	return true;
}

bool RestClient::updateTimeEntry(const Types::TimeEntry& entry, std::function<void(QRestReply&)> callback)
{
	if (nullptr == callback) {
		return false;
	}
	// FIXME Create to and from Json Method for Types::TimeEntry
	const auto json = QJsonDocument();
	auto reply = mRestAccesManager->put(mFactory.createRequest(), json, this, callback);
	connect(reply, &QNetworkReply::finished, this, &RestClient::replyFinished);

	return true;
}

} // namespace QRedmine
