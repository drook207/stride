#pragma once

#include "iauthentication.h"
#include <QObject>

namespace QRedmine::Authentication {

class PasswordAuthentication final : public IAuthentication {
	Q_OBJECT
public:
	explicit PasswordAuthentication(
		const QStringView& username, const QStringView& password, QObject* parent = nullptr);

private:
	QByteArray mUsername {};
	QByteArray mPassword {};

	// Authenticator interface
public:
	void addAuthenticationToRequest(QNetworkRequest& request) override;
};

} // namespace QRedmine
