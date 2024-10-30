#pragma once

#include "request/request.h"
#include <QJsonDocument>

namespace QRedmine::Requests::Json {

class JsonRequest : public Request {
public:
	explicit JsonRequest(QObject* parent = nullptr);
	virtual bool toJson() = 0;
	virtual bool fromJson() = 0;

	[[nodiscard]] QJsonDocument Data() const;
	void setData(const QJsonDocument& newData);

	/**
	 * Holds the JSON data for the request.
	 *
	 * This variable is utilized to store and manage the JSON representation of a request
	 * throughout the data flow in the system. The `mData` variable is a protected member
	 * and primarily used within derived classes that implement JSON specific behavior
	 * through the `JsonRequest` interface. It is initially empty and can be set and
	 * retrieved using the respective accessor and mutator methods.
	 *
	 * @sa QRedmine::JsonRequest
	 */
protected:
	QJsonDocument mData {};
};

} // namespace QRedmine
