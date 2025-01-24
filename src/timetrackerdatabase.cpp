#include "timetrackerdatabase.h"
#include <QDateTime>
#include <QDir>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <qcoreapplication.h>
#include <qstringliteral.h>

Q_LOGGING_CATEGORY(lcTimeEntryDataBase, "qredmine.timeentry.database")

namespace QRedmineClient {
bool TimeEntryDatabase::initialize()
{
	QDir applicationDir = QCoreApplication::instance()->applicationDirPath();
	QDir databaseDir(applicationDir);
	const QString databaseDirName = QStringLiteral("database");

	if (!databaseDir.exists(databaseDirName)) {
		databaseDir.mkdir(databaseDirName);
	}

	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
	qDebug(lcTimeEntryDataBase()) << "Connection name: " << db.connectionName();
	db.setDatabaseName("timeentries.db");

	if (!db.open()) {
		qWarning(lcTimeEntryDataBase) << "Failed to open database:" << db.lastError().text();
		return false;
	}

	QSqlQuery query;
	return query.exec("CREATE TABLE IF NOT EXISTS time_entries ("
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
}

bool TimeEntryDatabase::addTimeEntry(int issueId, double hours, const QString& comments)
{
	QSqlQuery query;
	query.prepare("INSERT INTO time_entries "
				  "(issue_id, hours, comments, spent_on, created_at, updated_at) "
				  "VALUES (?, ?, ?, ?, ?, ?)");

	QDateTime now = QDateTime::currentDateTime();
	query.addBindValue(issueId);
	query.addBindValue(hours);
	query.addBindValue(comments);
	query.addBindValue(now);
	query.addBindValue(now);
	query.addBindValue(now);

	return query.exec();
}

bool TimeEntryDatabase::updateSyncStatus(int id, int redmineId)
{
	QSqlQuery query;
	query.prepare("UPDATE time_entries "
				  "SET sync_status = 1, redmine_id = ?, updated_at = ? "
				  "WHERE id = ?");

	query.addBindValue(redmineId);
	query.addBindValue(QDateTime::currentDateTime());
	query.addBindValue(id);

	return query.exec();
}

QVector<QVariantMap> TimeEntryDatabase::getUnsyncedEntries()
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

QVector<QVariantMap> TimeEntryDatabase::getRecentEntries(int limit)
{
	QVector<QVariantMap> entries;
	QSqlQuery query;
	query.prepare("SELECT * FROM time_entries "
				  "ORDER BY created_at DESC "
				  "LIMIT ?");
	query.addBindValue(limit);

	if (query.exec()) {
		while (query.next()) {
			QVariantMap entry;
			entry["id"] = query.value("id");
			entry["issue_id"] = query.value("issue_id");
			entry["hours"] = query.value("hours");
			entry["comments"] = query.value("comments");
			entry["spent_on"] = query.value("spent_on");
			entry["sync_status"] = query.value("sync_status");
			entry["redmine_id"] = query.value("redmine_id");
			entries.append(entry);
		}
	}

	return entries;
}
}
