#pragma once

// Thanks to https://github.com/fathomssen/qtredmine for the structs

#include "resource.h"

namespace QRedmine::Types {

struct IssueStatus : Resource {
	int id = DEFAULT_ID;
	QString name;
	bool isClosed;
	bool isDefault;
};

} // namespace QRedmine
