#pragma once

#include "resource.h"

namespace QRedmine::Types {

struct Group : Resource {
	int id = DEFAULT_ID;
	QString name;
	Items members;
};

} // namespace QRedmine
