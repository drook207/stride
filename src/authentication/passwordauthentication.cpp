#include "passwordauthentication.h"
#include "qredmineclient_global.h"
#include <QLoggingCategory>

namespace QRedmine::Authentication {
Q_LOGGING_CATEGORY(lcPasswordAuthenticator, "qredmineclient.password authenticator")

PasswordAuthentication::PasswordAuthentication(
	const QStringView& username, const QStringView& password, QObject* parent)
	: IAuthentication(parent)
	, mUsername(username.toLatin1())
	, mPassword(password.toLatin1().toBase64())
{
}

void PasswordAuthentication::addAuthenticationToRequest(QNetworkRequest& request)
{
	const auto data = QByteArray(Global::BASIC_HTTP.latin1() + mUsername + ":" + mPassword);
	qDebug(lcPasswordAuthenticator) << "Authentication date=" << data;
	request.setRawHeader(Global::AUTHORIZATION.latin1(), data);
}

} // namespace QRedmine
