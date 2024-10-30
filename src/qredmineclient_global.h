#pragma once

#include <QString>

#if defined(QREDMINECLIENT_LIBRARY)
#define QREDMINECLIENT_EXPORT Q_DECL_EXPORT
#else
#define QREDMINECLIENT_EXPORT Q_DECL_IMPORT
#endif

namespace QRedmine::Global {

struct ConstLatin1String : public QLatin1String {
	explicit constexpr ConstLatin1String(const char* const s)
		: QLatin1String(s, static_cast<int>(std::char_traits<char>::length(s))) {};
};

constexpr auto CONTENT_TYPE_JSON = ConstLatin1String("Content-Type: application/json");
constexpr auto CONTENT_TYPE_XML = ConstLatin1String("Content-Type: application/xml");
constexpr auto AUTHORIZATION = ConstLatin1String("Authorization");
constexpr auto BASIC_HTTP = ConstLatin1String("Basic");
constexpr auto X_REDMINE_APP_KEY = ConstLatin1String("X-Redmine-API-Key");
constexpr qint32 DEFAULT_ID = -1;

}
