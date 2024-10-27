#pragma once

#include <QNetworkRequest>
#include <QObject>

namespace QRedmine {

class IAuthentication : public QObject {
	Q_OBJECT
public:
	explicit IAuthentication(QObject* parent = nullptr);

	virtual void addAuthenticationToRequest(QNetworkRequest& request) = 0;
};

} // namespace QRedmine
