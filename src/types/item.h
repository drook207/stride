#pragma once

#include "qredmineclient_global.h"
#include <QString>

namespace QRedmine::Types {

struct Item {
	int id = Global::DEFAULT_ID;
	QString name;
};

using Items = QList<Item>;
} // namespace QRedmine
