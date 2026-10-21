#include "datamigration.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include "appdatapath.h"

namespace Stride {
namespace DataMigration {

	const QString MARKER_FILE_NAME = QStringLiteral("migrated-to-stride.txt");

	static const QString LEGACY_ORGANIZATION = QStringLiteral("QRedmine");
	static const QString APPLICATION_NAME = QStringLiteral("Stride");
	static const QString DATABASE_FILE_NAME = QStringLiteral("timeentries.db");
	static const QStringList DATABASE_SUFFIXES { QString(), QStringLiteral("-wal"), QStringLiteral("-shm") };
	static const QStringList DATABASE_FILE_NAMES { DATABASE_FILE_NAME, DATABASE_FILE_NAME + QStringLiteral("-wal"),
		DATABASE_FILE_NAME + QStringLiteral("-shm") };
	static const QString TEMPORARY_SUFFIX = QStringLiteral(".migrating");

	QString legacyDirectory(const QString& directory)
	{
		const QFileInfo info(directory);
		if (info.fileName() != APPLICATION_NAME) {
			return {};
		}
		return QDir(info.absolutePath()).absoluteFilePath(LEGACY_ORGANIZATION + QLatin1Char('/') + info.fileName());
	}

	bool migrateSettings(const QSettings& from, QSettings& to)
	{
		const QStringList keys = from.allKeys();
		if (keys.isEmpty() || !to.allKeys().isEmpty()) {
			return false;
		}
		for (const QString& key : keys) {
			to.setValue(key, from.value(key));
		}
		to.sync();
		return to.status() == QSettings::NoError;
	}

	// Copies a file next to its destination under a temporary name, checks it and renames it, so a copy that
	// is aborted midway never leaves a partial file at the destination.
	static bool copyFile(const QString& from, const QString& to)
	{
		const QString temporary = to + TEMPORARY_SUFFIX;
		QFile::remove(temporary); // a leftover from an earlier, aborted attempt
		if (!QFile::copy(from, temporary) || QFileInfo(temporary).size() != QFileInfo(from).size() ||
			!QFile::rename(temporary, to)) {
			QFile::remove(temporary);
			return false;
		}
		return true;
	}

	// The database and its -wal/-shm files: all of them or none.
	static bool copyDatabase(const QString& from, const QString& to, Report& report)
	{
		QStringList copied;
		for (const QString& suffix : DATABASE_SUFFIXES) {
			const QString source = QDir(from).absoluteFilePath(DATABASE_FILE_NAME + suffix);
			if (!QFileInfo::exists(source)) {
				continue;
			}
			const QString target = QDir(to).absoluteFilePath(DATABASE_FILE_NAME + suffix);
			if (!copyFile(source, target)) {
				for (const QString& done : std::as_const(copied)) {
					QFile::remove(QDir(to).absoluteFilePath(DATABASE_FILE_NAME + done));
				}
				report.warnings << QStringLiteral("Could not copy %1 to %2").arg(source, target);
				return false;
			}
			copied << suffix;
		}
		if (!copied.isEmpty()) {
			report.info << QStringLiteral("Copied the database from %1 to %2").arg(from, to);
		}
		return !copied.isEmpty();
	}

	// Copies the files and subdirectories of from that don't exist in to. At the top level, the database files
	// are left to copyDatabase(). Returns whether everything that should be copied was.
	static bool copyTree(const QString& from, const QString& to, Report& report, bool& copiedAnything, bool topLevel)
	{
		if (!QDir().mkpath(to)) {
			report.warnings << QStringLiteral("Could not create %1").arg(to);
			return false;
		}
		bool ok = true;
		const QFileInfoList entries =
			QDir(from).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
		for (const QFileInfo& entry : entries) {
			const QString name = entry.fileName();
			const QString target = QDir(to).absoluteFilePath(name);
			if (entry.isDir()) {
				ok = copyTree(entry.absoluteFilePath(), target, report, copiedAnything, false) && ok;
			} else if (name.endsWith(TEMPORARY_SUFFIX) || name == MARKER_FILE_NAME ||
				(topLevel && DATABASE_FILE_NAMES.contains(name)) || QFileInfo::exists(target)) {
				continue;
			} else if (copyFile(entry.absoluteFilePath(), target)) {
				copiedAnything = true;
			} else {
				report.warnings << QStringLiteral("Could not copy %1 to %2").arg(entry.absoluteFilePath(), target);
				ok = false;
			}
		}
		return ok;
	}

	bool migrateDirectory(const QString& from, const QString& to, Report& report)
	{
		const QFileInfo source(from);
		if (from.isEmpty() || to.isEmpty() || source.absoluteFilePath() == QFileInfo(to).absoluteFilePath() ||
			!source.isDir() || QFileInfo::exists(QDir(from).absoluteFilePath(MARKER_FILE_NAME))) {
			return false;
		}

		bool copiedAnything = false;
		bool ok = QDir().mkpath(to);
		if (!ok) {
			report.warnings << QStringLiteral("Could not create %1").arg(to);
			return false;
		}
		// The database as a whole, and only if there is none yet: a leftover -wal file of the old database
		// must never end up next to another one
		if (!QFileInfo::exists(QDir(to).absoluteFilePath(DATABASE_FILE_NAME)) &&
			QFileInfo::exists(QDir(from).absoluteFilePath(DATABASE_FILE_NAME))) {
			if (copyDatabase(from, to, report)) {
				copiedAnything = true;
			} else {
				ok = false;
			}
		}
		ok = copyTree(from, to, report, copiedAnything, true) && ok;

		// Only a complete copy is final; else the next start tries again for what is missing
		if (ok) {
			QFile marker(QDir(from).absoluteFilePath(MARKER_FILE_NAME));
			if (marker.open(QIODevice::WriteOnly | QIODevice::Text)) {
				marker.write(QStringLiteral("Copied to %1 on the rename to Stride. This directory is no longer used "
											"and can be deleted.\n")
								 .arg(to)
								 .toUtf8());
			}
		}
		if (copiedAnything) {
			report.info << QStringLiteral("Copied the data in %1 to %2").arg(from, to);
		}
		return copiedAnything;
	}

	Report migrateLegacyApplicationData()
	{
		Report report;

		QSettings legacy(LEGACY_ORGANIZATION, APPLICATION_NAME);
		QSettings current;
		if (migrateSettings(legacy, current)) {
			report.info
				<< QStringLiteral("Copied the settings from %1 to %2").arg(legacy.fileName(), current.fileName());
		}

		// Windows has the database (roaming) and the logs (local) in different places, elsewhere they're the same
		QStringList done;
		for (const auto location : { QStandardPaths::AppDataLocation, QStandardPaths::AppLocalDataLocation }) {
			const QString directory = QRedmineClient::Utils::appDataPath(location);
			if (done.contains(directory)) {
				continue;
			}
			done << directory;
			migrateDirectory(legacyDirectory(directory), directory, report);
		}
		return report;
	}

} // namespace DataMigration
} // namespace Stride
