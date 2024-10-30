#include "request.h"
#include "QLoggingCategory"

namespace QRedmine::Requests {

Q_LOGGING_CATEGORY(lcRequest, "qredmineclient.request")

Request::Request(QObject* parent)
	: QObject(parent)
{
}

QString Request::Resource() const { return mResource; }

void Request::setReply(QNetworkReply* newReply)
{
	if (nullptr != newReply) {
		mReply = newReply;
		return;
	}

	qWarning(lcRequest) << "This request is already ongoing.";
}

void Request::setParameters(const QString& newParameters) { mParameters = newParameters; }

QString Request::Parameters() const { return mParameters; }

QNetworkRequest Request::NetworkRequest() const { return mRequest; }

QNetworkAccessManager::Operation Request::Mode() const { return mMode; }

QByteArray Request::PostData() const { return mPostData; }

void Request::handleNetworkError(QNetworkReply::NetworkError networkError)
{
	qWarning(lcRequest) << "Network error occurred: " << networkError;
	emit error();
}

void Request::resourceRequestMode()
{
	if (mId == Global::DEFAULT_ID) {
		mMode = QNetworkAccessManager::PostOperation;
		return;
	}
	mMode = QNetworkAccessManager::PutOperation;
	mResource.append("/" + QString::number(mId));
}

} // namespace QRedmine
