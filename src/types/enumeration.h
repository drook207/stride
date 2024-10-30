#pragma once
#include "resource.h"
#include <QString>

namespace QRedmine::Types {

struct Enumeration : Resource {
	int id = DEFAULT_ID;
	QString name;
	bool isDefault;
};

} // namespace QRedmine
