#include "database.h"
#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <qcoreapplication.h>
#include <qjsonobject.h>
#include <qstringliteral.h>
#include <utility>

Q_LOGGING_CATEGORY(lcDataBase, "qredmine.timeentry.database")

namespace QRedmineClient {
bool Database::initialize()
{
	QDir tmpDir;
	const auto homePath = QDir::homePath();
#ifdef Q_OS_WIN

	tmpDir = QDir(homePath + "/AppData/Local"); // QCoreApplication::instance()->applicationDirPath();
#endif
#ifdef Q_OS_LINUX
	tmpDir = QDir(homePath + "/.config/");
#endif

	const QString databaseDirName = QCoreApplication::instance()->applicationName(); // QStringLiteral("database");

	if (!tmpDir.exists(databaseDirName)) {
		tmpDir.mkdir(databaseDirName);
	}
	QDir databaseDir(tmpDir.absoluteFilePath(databaseDirName));

	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
	const auto databaseName = databaseDir.absoluteFilePath("timeentries.db");
	db.setDatabaseName(databaseName);

	if (!db.open()) {
		qWarning(lcTimeEntryDataBase) << "Failed to open database:" << db.lastError().text();
		return false;
	}

	QSqlQuery query;
	bool status = false;
	status &= query.exec("CREATE TABLE IF NOT EXISTS enum_time_entry_activities ("
						 "enum_id INTEGER NOT NULL,"
						 "is_default BOOLEAN NOT NULL,"
						 "name TEXT NOT NULL"
						 ")");
	status &= query.exec("CREATE TABLE IF NOT EXISTS time_entries ("
						 "id INTEGER PRIMARY KEY AUTOINCREMENT,"
						 "issue_id INTEGER NOT NULL,"
						 "hours REAL NOT NULL,"
						 "comments TEXT,"
						 "spent_on DATETIME NOT NULL,"
						 "sync_status INTEGER DEFAULT 0," // 0 = not synced, 1 = synced
						 "redmine_id INTEGER," // ID from Redmine after sync
						 "created_at DATETIME NOT NULL,"
						 "updated_at DATETIME NOT NULL"
						 ")");

	return status;
}

bool Database::addTimeEntry(int issueId, double hours, const QString& comments, const QDateTime& spentOn)
{
	QSqlQuery query;
	query.prepare("INSERT INTO time_entries "
				  "(issue_id, hours, comments, spent_on, created_at, updated_at) "
				  "VALUES (?, ?, ?, ?, ?, ?)");

	QDateTime now = QDateTime::currentDateTime();
	query.addBindValue(issueId);
	query.addBindValue(hours);
	query.addBindValue(comments);
	query.addBindValue(spentOn);
	query.addBindValue(now);
	query.addBindValue(now);

	return query.exec();
}

bool Database::updateSyncStatus(int id, int redmineId)
{
	QSqlQuery query;
	if (!query.prepare("UPDATE time_entries "
					   "SET sync_status = 1, redmine_id = ?, updated_at = ? "
					   "WHERE id = ?")) {
		return false;
	}

	query.addBindValue(redmineId);
	query.addBindValue(QDateTime::currentDateTime());
	query.addBindValue(id);

	return query.exec();
}

bool Database::addTimeEntryActivities(const QJsonArray& activities)
{
	QSqlQuery query;
	bool status = false;
	if (!query.prepare("INSERT INTO enum_time_entry_activities "
					   "(enum_id, is_default, name) "
					   "VALUES (?, ?, ?)")) {
		return false;
	}

	for (const auto& activity : std::as_const(activities)) {
		QJsonObject obj = activity.toObject();
		const auto id = obj["id"].toInt();
		const auto is_default = obj["is_default"].toBool();
		const auto name = obj["name"].toString();
		query.addBindValue(id);
		query.addBindValue(is_default);
		query.addBindValue(name);
		status &= query.exec();
	}
	return status;
}

QVector<QVariantMap> Database::getUnsyncedEntries()
{
	QVector<QVariantMap> entries;
	QSqlQuery query("SELECT * FROM time_entries WHERE sync_status = 0");

	while (query.next()) {
		QVariantMap entry;
		entry["id"] = query.value("id");
		entry["issue_id"] = query.value("issue_id");
		entry["hours"] = query.value("hours");
		entry["comments"] = query.value("comments");
		entry["spent_on"] = query.value("spent_on");
		entries.append(entry);
	}

	return entries;
}

QJsonArray Database::getRecentEntries(int limit)
{
	QJsonArray entries;
	QSqlQuery query;
	query.prepare("SELECT * FROM time_entries "
				  "ORDER BY created_at DESC "
				  "LIMIT ?");
	query.addBindValue(limit);
	if (query.exec()) {
		while (query.next()) {
			QJsonObject entry;
			entry["id"] = query.value("id").toInt();
			entry["issue_id"] = query.value("issue_id").toInt();
			entry["hours"] = query.value("hours").toDouble();
			entry["comments"] = query.value("comments").toString();
			entry["spent_on"] = query.value("spent_on").toString();
			entry["sync_status"] = query.value("sync_status").toInt();
			entry["redmine_id"] = query.value("redmine_id").toInt();
			entries.append(entry);
		}
	}
	return entries;
}
}
