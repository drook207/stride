#pragma once
// Thanks to https://github.com/fathomssen/qtredmine for the structs
#include "resource.h"

namespace QRedmine::Types {

struct Project : Resource {
	int id = Global::DEFAULT_ID;

	QString description;
	QString identifier;
	bool isPublic;
	QString name;
	Item parent;

	Items trackers;
	Items categories;
};

} // namespace QRedmine
