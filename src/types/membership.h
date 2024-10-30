#pragma once

#include "resource.h"

namespace QRedmine::Types {

struct Membership : Resource {
	int id = DEFAULT_ID;
	Item project;
	Item user;
	Item group;
	Items roles;
};

} // namespace QRedmine
