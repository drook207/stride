#pragma once

#include <QCoreApplication>
#include <QStandardPaths>
#include <QString>

namespace QRedmineClient {
namespace Utils {

	/*!
	 * \brief Directory the application keeps its data in (database, logs).
	 *
	 * This is QStandardPaths::writableLocation() of \a location, except that an organization name equal to the
	 * application name is not repeated: Qt appends both, which would give e.g. ~/.local/share/Stride/Stride
	 * (Windows: %LOCALAPPDATA%\Stride\Stride) for the organization "Stride" and the application "Stride"
	 * (which is what makes QSettings use ~/.config/Stride/Stride.conf, see main.cpp of the app).
	 *
	 * \param location AppLocalDataLocation (the default) or AppDataLocation, which differ on Windows (Local vs.
	 * Roaming AppData)
	 * \return The directory (not necessarily existing yet), e.g. ~/.local/share/Stride
	 */
	static inline QString appDataPath(QStandardPaths::StandardLocation location = QStandardPaths::AppLocalDataLocation)
	{
		QString path = QStandardPaths::writableLocation(location);
		const QString organization = QCoreApplication::organizationName();
		const QString duplicate =
			QLatin1Char('/') + organization + QLatin1Char('/') + QCoreApplication::applicationName();
		if (!organization.isEmpty() && organization == QCoreApplication::applicationName() &&
			path.endsWith(duplicate)) {
			path.chop(organization.size() + 1);
		}
		return path;
	}

} // namespace Utils
} // namespace QRedmineClient
