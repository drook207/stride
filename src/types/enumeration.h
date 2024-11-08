#pragma once
// Thanks to https://github.com/fathomssen/qtredmine for the structs
#include "resource.h"
#include <QString>

namespace QRedmine::Types {

struct Enumeration : Resource {
	int id = Global::DEFAULT_ID;
	QString name;
	bool isDefault;
};

} // namespace QRedmine
