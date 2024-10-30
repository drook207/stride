#include "tokenauthentication.h"
#include "qredmineclient_global.h"
#include <QLoggingCategory>

namespace QRedmine::Authentication {

Q_LOGGING_CATEGORY(lcTokenAuthenticator, "qredmineclient.token authenticator")

TokenAuthentication::TokenAuthentication(const QStringView& apiKey, QObject* parent)
	: IAuthentication(parent)
	, mApiKey(apiKey.toLatin1())
{
}

void TokenAuthentication::addAuthenticationToRequest(QNetworkRequest& request)
{
	request.setRawHeader(Global::X_REDMINE_APP_KEY.latin1(), mApiKey);
}

} // namespace QRedmine
