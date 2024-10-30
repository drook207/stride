#pragma once

#include "customfield.h"
#include "item.h"
#include "resource.h"

namespace QRedmine::Types {

struct TimeEntry : Resource {
	Item activity;
	QString comment;
	double hours = 0;
	Item issue;
	Item project;
	QDate spentOn;
	CustomFields customFields;
};
} // namespace QRedmine
