#include "timetrackerdatabase.h"
#include <QDateTime>
#include <QDir>
#include <QJsonObject>
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

	const QString databaseDirName = QStringLiteral("database");

	if (!applicationDir.exists(databaseDirName)) {
		applicationDir.mkdir(databaseDirName);
	}
	QDir databaseDir(applicationDir.absoluteFilePath(databaseDirName));

	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
	const auto databaseName = databaseDir.absoluteFilePath("timeentries.db");
	db.setDatabaseName(databaseName);

	if (!db.open()) {
		qWarning(lcTimeEntryDataBase) << "Failed to open database:" << db.lastError().text();
		return false;
	}

	QSqlQuery query;
	if (!query.exec("CREATE TABLE IF NOT EXISTS time_entries ("
					"id INTEGER PRIMARY KEY AUTOINCREMENT,"
					"issue_id INTEGER NOT NULL,"
					"hours REAL NOT NULL,"
					"comments TEXT,"
					"spent_on DATETIME NOT NULL,"
					"sync_status INTEGER DEFAULT 0," // 0 = not synced, 1 = synced
					"redmine_id INTEGER," // ID from Redmine after sync
					"created_at DATETIME NOT NULL,"
					"updated_at DATETIME NOT NULL"
					")")) {
		qWarning() << QStringLiteral("Could not create time_entries table: %0").arg(query.lastError().text());
		return false;
	}

	return createIssuesDatabase();
}

bool TimeEntryDatabase::createIssuesDatabase()
{
	QSqlQuery query;
	if (!query.prepare("CREATE TABLE IF NOT EXISTS issues ("
					   "id INTEGER PRIMARY KEY NOT NULL, "
					   "project_id INTEGER, "
					   "status_id INTEGER NOT NULL, "
					   "description TEXT, "
					   "created_on DATETIME NOT NULL, "
					   "updated_on DATETIME NOT NULL)")) {
		qWarning() << QStringLiteral("Could not prepare issues table creation query: %0").arg(query.lastError().text());
		return false;
	}

	if (!query.exec()) {
		qWarning() << QStringLiteral("Could not execute issues table creation query: %0").arg(query.lastError().text());
		return false;
	}

	return true;
}

bool TimeEntryDatabase::addIssue(int issueId, int projectId, int statusId, const QString& description,
	const QDateTime& createdOn, const QDateTime& updatedOn)
{
	QSqlQuery query;
	// INSERT OR REPLACE
	if (!query.prepare("REPLACE INTO issues ("
					   "id, project_id, status_id, description, created_on, updated_on) VALUES "
					   "(:id,:project_id,:status_id,:description,:created_on,:updated_on)")) {
		qWarning() << QStringLiteral("Could not prepare issue creation query: %0").arg(query.lastError().text());
		return false;
	}

	query.bindValue(":id", issueId);
	query.bindValue(":project_id", projectId);
	query.bindValue(":status_id", statusId);
	query.bindValue(":description", description);
	query.bindValue(":created_on", createdOn);
	query.bindValue(":updated_on", updatedOn);

	if (!query.exec()) {
		qWarning() << QStringLiteral("Could not execute issue creation query: %0").arg(query.lastError().text());
		return false;
	}

	return true;
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

QJsonArray TimeEntryDatabase::getRecentEntries(int limit)
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
