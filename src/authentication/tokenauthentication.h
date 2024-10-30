#pragma once

#include "iauthentication.h"
#include <QObject>

namespace QRedmine::Authentication {

class TokenAuthentication final : public IAuthentication {
	Q_OBJECT
public:
	explicit TokenAuthentication(const QStringView& apiKey, QObject* parent = nullptr);

private:
	QByteArray mApiKey {};

	// Authenticator interface
public:
	void addAuthenticationToRequest(QNetworkRequest& request) override;
};

} // namespace QRedmine
