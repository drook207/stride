#pragma once

#include "item.h"
#include "qredmineclient_global.h"

namespace QRedmine::Types {

struct IssueCategory {
	int id = DEFAULT_ID;
	QString name;
	Item project;
	Item assignedTo;
};

} // namespace QRedmine
