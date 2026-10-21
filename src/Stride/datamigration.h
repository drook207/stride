#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>

namespace Stride {

/*!
 * \brief One-time migration of the application's settings and data from the location used before the
 * application was renamed to Stride.
 *
 * Before, the organization name was "QRedmine" (settings in ~/.config/QRedmine/Stride.conf or the registry
 * key HKCU\Software\QRedmine\Stride, data in ~/.local/share/QRedmine/Stride or %LOCALAPPDATA%\QRedmine\Stride
 * and %APPDATA%\QRedmine\Stride); now the organization name is "Stride" and the data directory has no
 * organization level (see Utils::appDataPath()). The old files are copied, never moved or overwritten:
 * what already exists at the new location is kept, and the old location is left as it was (plus a marker
 * file, so a copy is never made twice).
 */
namespace DataMigration {

	//! File written to a legacy data directory once its content has been copied
	extern const QString MARKER_FILE_NAME;

	/*!
	 * \brief What a migration did, for the log (which isn't open yet while the migration runs).
	 */
	struct Report {
		//! What was copied
		QStringList info;
		//! What could not be copied
		QStringList warnings;
	};

	/*!
	 * \brief legacyDirectory The directory the application's data was kept in before the rename.
	 * \param directory The data directory now, e.g. ~/.local/share/Stride
	 * \return The same with the organization "QRedmine" in between, e.g. ~/.local/share/QRedmine/Stride, or an
	 * empty string where the data directory has no such layout (e.g. Android)
	 */
	QString legacyDirectory(const QString& directory);

	/*!
	 * \brief migrateSettings Copies all keys of from to to, unless to already has settings. from is not
	 * changed.
	 * \return Whether keys were copied
	 */
	bool migrateSettings(const QSettings& from, QSettings& to);

	/*!
	 * \brief migrateDirectory Copies the files and subdirectories of from that don't exist in to into to.
	 * The database (with its write-ahead log files) is copied only as a whole and only if to has none.
	 * Every file is copied under a temporary name and renamed once complete, so an aborted copy never
	 * leaves a partial file behind that a later start would take for a finished one. Does nothing if from
	 * carries the marker file; writes it after a copy without problems.
	 * \param report Receives what happened
	 * \return Whether anything was copied
	 */
	bool migrateDirectory(const QString& from, const QString& to, Report& report);

	/*!
	 * \brief migrateLegacyApplicationData Runs the whole migration for the current application identity
	 * (QCoreApplication's organization and application name): the settings of "QRedmine"/"Stride" and the
	 * data directories (database, logs). Call before anything reads the settings or opens the log or the
	 * database.
	 */
	Report migrateLegacyApplicationData();

} // namespace DataMigration
} // namespace Stride
