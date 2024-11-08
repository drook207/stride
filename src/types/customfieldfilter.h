#pragma once
// Thanks to https://github.com/fathomssen/qtredmine for the structs
#include "qredmineclient_global.h"
#include <QString>

namespace QRedmine::Types {

struct CustomFieldFilter {
	int projectId = Global::DEFAULT_ID;
	int trackerId = Global::DEFAULT_ID;
	QString type;
	QString format;
};

} // namespace QRedmine
