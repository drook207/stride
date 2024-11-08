#pragma once
// Thanks to https://github.com/fathomssen/qtredmine for the structs
#include "item.h"
#include <QDateTime>
#include <QSharedData>

namespace QRedmine::Types {
struct Resource : QSharedData {
	QDateTime createdOn;
	QDateTime updatedOn;
	Item user;
};

} // namespace QRedmine
