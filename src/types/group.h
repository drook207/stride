#pragma once

// Thanks to https://github.com/fathomssen/qtredmine for the structs

#include "resource.h"

namespace QRedmine::Types {

struct Group : Resource {
	int id = Global::DEFAULT_ID;
	QString name;
	Items members;
};

} // namespace QRedmine
