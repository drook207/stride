#pragma once
// Thanks to https://github.com/fathomssen/qtredmine for the structs

#include "resource.h"

namespace QRedmine::Types {

struct Membership : Resource {
	int id = Global::DEFAULT_ID;
	Item project;
	Item user;
	Item group;
	Items roles;
};

} // namespace QRedmine
