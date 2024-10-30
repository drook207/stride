#pragma once

#include <QString>
#include <utility>

namespace QRedmine::Types {

struct RedmineOptions {
	explicit RedmineOptions(QString parameters = QLatin1String(), const bool getAllItems = false)
		: parameters(std::move(parameters))
		, getAllItems(getAllItems)
	{
	}

	QString parameters;
	bool getAllItems = false;
};

} // namespace QRedmine
