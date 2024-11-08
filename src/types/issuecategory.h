#pragma once

// Thanks to https://github.com/fathomssen/qtredmine for the structs

#include "item.h"
#include "qredmineclient_global.h"

namespace QRedmine::Types {

struct IssueCategory {
	int id = Global::DEFAULT_ID;
	QString name;
	Item project;
	Item assignedTo;
};

} // namespace QRedmine
