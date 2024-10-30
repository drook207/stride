#pragma once

#include "item.h"
#include <QDateTime>

namespace QRedmine::Types {
struct Resource {
	QDateTime createdOn;
	QDateTime updatedOn;
	Item user;
};

} // namespace QRedmine
