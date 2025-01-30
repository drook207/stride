#pragma once

#include <QSettings>
#include <QString>
#include <QVariant>

namespace QRedmineClient {
namespace Utils {

	static inline void SaveSetting(const QString& key, const QVariant& value)
	{
		QSettings settings;
		settings.setValue(key, value);
	}

	static inline QVariant GetSetting(const QString& key, const QVariant& defaultValue = QVariant())
	{
		QSettings settings;
		auto result = settings.value(key);
		return result.isValid() ? result : defaultValue;
	}

} // namespace Utils
} // namespace QRedmineClient
