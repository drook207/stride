#pragma once
// Thanks to https://github.com/fathomssen/qtredmine for the structs
#include "resource.h"

namespace QRedmine::Types {

struct Tracker : Resource {
	int id = Global::DEFAULT_ID;
	QString name;
};

} // namespace QRedmine
