#pragma once

#include "resource.h"

namespace QRedmine::Types {

struct Tracker : Resource {
	int id = DEFAULT_ID;
	QString name;
};

} // namespace QRedmine
