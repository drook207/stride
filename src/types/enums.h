#pragma once

namespace QRedmine::Types::Enums {
/// Redmine error codes
enum class RedmineError {
	NO_ERR,
	ERR_INCOMPLETE_DATA,
	ERR_NETWORK,
	ERR_NOT_SAVED,
	ERR_TIME_ENTRY_TOO_SHORT,
	ERR_TIMEOUT,
};

enum class VersionStatus {
	OPEN,
	LOCKED,
	CLOSED,
};

enum class VersionSharing {
	NONE,
	DESCENDANTS,
	HIERARCHY,
	TREE,
	SYSTEM,
};

}
