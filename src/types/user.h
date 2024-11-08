#pragma once
// Thanks to https://github.com/fathomssen/qtredmine for the structs
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
