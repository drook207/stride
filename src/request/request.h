#pragma once

#include "qredmineclient_global.h"
#include <QNetworkReply>

namespace QRedmine::Requests {

/**
 * @class Request
 *
 * @brief Represents a base class for HTTP requests in the application.
 *
 * The Request class provides a common interface and functionality for
 * various types of HTTP requests. It manages request parameters,
 * reply handling, and network operations. Derived classes are expected
 * to implement specific behaviors for different types of requests.
 */
class Request : public QObject {
	Q_OBJECT
public:
	explicit Request(QObject* parent = nullptr);

	[[nodiscard]] QString Resource() const;

	void setReply(QNetworkReply* newReply);

	void setParameters(const QString& newParameters);

	[[nodiscard]] QString Parameters() const;

	[[nodiscard]] QNetworkRequest NetworkRequest() const;

	[[nodiscard]] QNetworkAccessManager::Operation Mode() const;
	[[nodiscard]] QByteArray PostData() const;

	virtual bool isValidToSend() = 0;

public slots:
	virtual void requestFinished() = 0;
	void handleNetworkError(QNetworkReply::NetworkError networkError);

protected:
	QNetworkRequest mRequest {};
	QNetworkReply* mReply {};
	qint32 mId = Global::DEFAULT_ID;
	QString mResource {};
	QString mParameters {};
	QNetworkAccessManager::Operation mMode {};
	QByteArray mPostData {};
	bool mIsValidToSend = false;
	void resourceRequestMode();

signals:
	void finished();
	void error();
	void timeout();
};

} // namespace QRedmine
