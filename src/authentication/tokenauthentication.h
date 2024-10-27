#pragma once

#include "iauthentication.h"
#include <QObject>

namespace QRedmine {

class TokenAuthentication : public IAuthentication {
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
