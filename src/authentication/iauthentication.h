#pragma once

#include <QNetworkRequest>
#include <QObject>

namespace QRedmine::Authentication {

class IAuthentication : public QObject {
	Q_OBJECT
public:
	explicit IAuthentication(QObject* parent = nullptr);

	virtual void addAuthenticationToRequest(QNetworkRequest& request) = 0;
};

} // namespace QRedmine
