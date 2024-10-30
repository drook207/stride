#pragma once

#include "authentication/iauthentication.h"
#include "qredmineclient_global.h"
#include "request/json/timeentryrequestjson.h"
#include "request/request.h"
#include <QNetworkAccessManager>
#include <QObject>

namespace QRedmine {
class QREDMINECLIENT_EXPORT Client final : public QObject {
	Q_OBJECT
public:
	explicit Client(QObject* parent = nullptr);

	explicit Client(QUrl url, QObject* parent = nullptr);

	Client(QUrl url, const QStringView& apiKey, bool sslVerify = true, QObject* parent = nullptr);

	Client(QUrl url, const QStringView& username, const QStringView& password, bool sslVerify = true,
		QObject* parent = nullptr);

	[[nodiscard]] bool setAuthenticationMethod(const QStringView& apiKey);
	[[nodiscard]] bool setAuthenticationMethod(const QStringView& username, const QStringView& password);

	void reconnect();

	[[nodiscard]] QUrl Url() const;
	void setUrl(const QUrl& newUrl);

	[[nodiscard]] bool SslVerify() const;
	void setSslVerify(bool newSslVerify);

	[[nodiscard]] QString UserAgent() const;
	void setUserAgent(const QString& newUserAgent);

public slots:

	bool timeEntry(Requests::Json::TimeEntryRequestJson& request, qint32 id);

	bool setTimeEntry(Requests::Json::TimeEntryRequestJson& request, qint32 id);

	void handleSslError(QNetworkReply* reply, const QList<QSslError>& errors) const;

	static void replyFinished(QNetworkReply* reply);
signals:
	void initialized();

private:
	void init();
	bool sendRequest(int id, Requests::Request* request);
	QNetworkAccessManager* mManager {};

	QUrl mUrl {};
	bool mSslVerify = true;
	Authentication::IAuthentication* mAuthenticator {};
	QString mUserAgent {};
};
} // namespace QRedmine
