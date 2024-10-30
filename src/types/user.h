#pragma once

#include "resource.h"

namespace QRedmine::Types {

struct User : Resource {
	int id = DEFAULT_ID;

	QString login;
	QString firstname;
	QString lastname;

	QString mail;
	QDateTime lastLoginOn;
};

} // namespace QRedmine
