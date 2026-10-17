#include "database.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <qcontainerfwd.h>
#include <qcoreapplication.h>
#include <qjsonobject.h>
#include <qstringliteral.h>

Q_LOGGING_CATEGORY(lcDataBase, "qredmine.timeentry.database")

namespace QRedmineClient {

static bool sInitialized = false;

static const QString DATABASE_FILE_NAME = QStringLiteral("timeentries.db");

QString Database::defaultPath()
{
	return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absoluteFilePath(DATABASE_FILE_NAME);
}

// Where the database was kept before it moved to QStandardPaths::AppDataLocation: a directory named
// after the application in ~/.config (Linux) or ~/AppData/Local (Windows). Elsewhere it ended up in
// the working directory.
static QString legacyPath()
{
	QString base;
#ifdef Q_OS_WIN
	base = QDir::homePath() + QStringLiteral("/AppData/Local");
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
	base = QDir::homePath() + QStringLiteral("/.config");
#else
	return QDir::current().absoluteFilePath(QCoreApplication::applicationName() + "/" + DATABASE_FILE_NAME);
#endif
	return QDir(base).absoluteFilePath(QCoreApplication::applicationName() + "/" + DATABASE_FILE_NAME);
}

// Moves a database from its legacy location (with SQLite's -wal/-shm companions) to path, unless
// there already is one at path.
static void moveLegacyDatabase(const QString& path)
{
	const QString legacy = legacyPath();
	if (legacy == path || !QFile::exists(legacy) || QFile::exists(path)) {
		return;
	}
	for (const QString& suffix : { QString(), QStringLiteral("-wal"), QStringLiteral("-shm") }) {
		if (!QFile::exists(legacy + suffix)) {
			continue;
		}
		if (!QFile::rename(legacy + suffix, path + suffix) &&
			!(QFile::copy(legacy + suffix, path + suffix) && QFile::remove(legacy + suffix))) {
			qWarning(lcDataBase) << "Could not move the database from" << legacy << "to" << path;
			return;
		}
	}
	QDir().rmdir(QFileInfo(legacy).absolutePath()); // only if nothing else is left in it
	qInfo(lcDataBase) << "Moved the database from" << legacy << "to" << path;
}

bool Database::initialize(const QString& path)
{
	if (sInitialized) {
		return true;
	}

	const QString databaseName = path.isEmpty() ? defaultPath() : path;
	if (path.isEmpty()) {
		QDir().mkpath(QFileInfo(databaseName).absolutePath());
		moveLegacyDatabase(databaseName);
	}

	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
	db.setDatabaseName(databaseName);
	if (!db.open()) {
		qWarning(lcDataBase) << QStringLiteral("Failed to open database: <%0>").arg(db.lastError().text());
		return false;
	}

	if (!execute(SqlQueries::ENABLE_WAL_MODE) || !migrate()) {
		return false;
	}

	sInitialized = true;
	return true;
}

void Database::close()
{
	{
		QSqlDatabase db = QSqlDatabase::database(QSqlDatabase::defaultConnection, false);
		if (db.isValid()) {
			db.close();
		}
	}
	QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
	sInitialized = false;
}

int Database::schemaVersion()
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::GET_SCHEMA_VERSION, query) || !evaluateQueryExec(query) || !query.next()) {
		return -1;
	}
	return query.value(0).toInt();
}

bool Database::execute(const QString& sql)
{
	QSqlQuery query;
	return evaluateQueryPrepare(sql, query) && evaluateQueryExec(query);
}

// Migration n brings the schema from version n to n + 1. Append new ones; never change one that
// has been released, since existing databases have already run it.
const QList<bool (*)()>& Database::migrations()
{
	static const QList<bool (*)()> list { &Database::migrateToVersion1 };
	return list;
}

bool Database::migrate()
{
	const int version = schemaVersion();
	if (version < 0) {
		return false;
	}
	const auto& steps = migrations();
	if (version > steps.size()) {
		qWarning(lcDataBase) << "Database schema version" << version << "is newer than this app knows" << steps.size()
							 << "- was it opened by a newer version?";
		return true;
	}
	QSqlDatabase db = QSqlDatabase::database();
	for (int next = version; next < steps.size(); ++next) {
		db.transaction();
		// PRAGMA doesn't take bound values; the version is an int, so formatting it in is safe.
		if (!steps[next]() || !execute(SqlQueries::SET_SCHEMA_VERSION.arg(next + 1))) {
			db.rollback();
			qWarning(lcDataBase) << "Database migration to version" << next + 1 << "failed";
			return false;
		}
		db.commit();
		qCInfo(lcDataBase) << "Database migrated to schema version" << next + 1;
	}
	return true;
}

// The schema before versioning existed. Databases from then have version 0 too, so everything is
// created only if missing, and the issues' project_name column (added later) is added if missing.
bool Database::migrateToVersion1()
{
	if (!execute(SqlQueries::CREATE_TIME_ENTRY_TABLE) || !execute(SqlQueries::CREATE_TIME_ENTRIES_CREATED_AT_INDEX) ||
		!execute(SqlQueries::CREATE_TIME_ENTRIES_SYNC_STATUS_INDEX) || !execute(SqlQueries::CREATE_ISSUES_TABLE)) {
		return false;
	}

	QSqlQuery columns;
	if (!evaluateQueryPrepare(SqlQueries::GET_ISSUES_COLUMNS, columns) || !evaluateQueryExec(columns)) {
		return false;
	}
	bool hasProjectName = false;
	while (columns.next()) {
		hasProjectName = hasProjectName || columns.value("name").toString() == QStringLiteral("project_name");
	}
	return hasProjectName || execute(SqlQueries::ADD_ISSUES_PROJECT_NAME_COLUMN);
}

QList<int> Database::issueIdsWithoutProject(int limit)
{
	QList<int> ids;
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::GET_ISSUE_IDS_WITHOUT_PROJECT, query)) {
		return ids;
	}
	query.addBindValue(limit);
	if (!evaluateQueryExec(query)) {
		return ids;
	}
	while (query.next()) {
		ids.append(query.value(0).toInt());
	}
	return ids;
}

bool Database::addIssue(int issueId, const QString& subject, int projectId, const QString& projectName, int statusId,
	const QString& description, const QDateTime& createdOn, const QDateTime& updatedOn)
{
	QSqlQuery query;
	// INSERT OR REPLACE
	if (!evaluateQueryPrepare(SqlQueries::REPLACE_ISSUE, query)) {
		return false;
	}

	query.bindValue(":id", issueId);
	query.bindValue(":subject", subject);
	query.bindValue(":project_id", projectId);
	query.bindValue(":project_name", projectName);
	query.bindValue(":status_id", statusId);
	query.bindValue(":description", description);
	query.bindValue(":created_on", createdOn);
	query.bindValue(":updated_on", updatedOn);

	if (!evaluateQueryExec(query)) {
		return false;
	}

	return true;
}

QSqlQuery Database::getIssues()
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::GET_ISSUES, query)) {
		return QSqlQuery();
	}
	if (!evaluateQueryExec(query)) {
		return QSqlQuery();
	}

	return query;
}

int Database::addTimeEntry(int issueId, double hours, const QString& comments, const QDateTime& spentOn)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::ADD_TIME_ENTRY, query)) {
		return -1;
	}

	QDateTime now = QDateTime::currentDateTime();
	query.addBindValue(issueId);
	query.addBindValue(hours);
	query.addBindValue(comments);
	query.addBindValue(spentOn);
	query.addBindValue(now);
	query.addBindValue(now);

	if (!evaluateQueryExec(query)) {
		return -1;
	}
	return query.lastInsertId().toInt();
}

int Database::updateTimeEntry(int id, double hours, const QString& comments, const QDateTime& spentOn)
{
	return updateTimeEntries(QStringLiteral("id"), id, hours, comments, spentOn);
}

int Database::updateTimeEntriesByRedmineId(
	int redmineId, double hours, const QString& comments, const QDateTime& spentOn)
{
	return updateTimeEntries(QStringLiteral("redmine_id"), redmineId, hours, comments, spentOn);
}

int Database::updateTimeEntries(
	const QString& keyColumn, int key, double hours, const QString& comments, const QDateTime& spentOn)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::UPDATE_TIME_ENTRIES_WHERE + keyColumn + QStringLiteral(" = ?"), query)) {
		return -1;
	}

	query.addBindValue(hours);
	query.addBindValue(comments);
	query.addBindValue(spentOn);
	query.addBindValue(QDateTime::currentDateTime());
	query.addBindValue(key);

	if (!evaluateQueryExec(query)) {
		return -1;
	}
	return query.numRowsAffected();
}

bool Database::updateSyncStatus(int id, int redmineId)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::UPDATE_SYNC_STATE, query)) {
		return false;
	}

	query.addBindValue(redmineId);
	query.addBindValue(QDateTime::currentDateTime());
	query.addBindValue(id);

	if (!evaluateQueryExec(query)) {
		return false;
	}
	return true;
}

bool Database::evaluateQueryPrepare(const QStringView& sqlCommand, QSqlQuery& query)
{
	if (sqlCommand.isEmpty()) {
		qWarning(lcDataBase()) << QStringLiteral("Empty sql command");
		return false;
	}
	if (!query.prepare(sqlCommand.toString())) {
		qWarning(lcDataBase()) << QStringLiteral("SQL Query prepare <%0> failed with: <%1>")
									  .arg(query.lastQuery(), query.lastError().text());
		return false;
	}

	return true;
}

bool Database::evaluateQueryExec(QSqlQuery& query)
{
	if (!query.exec()) {
		qWarning(lcDataBase())
			<< QStringLiteral("SQL Query exec <%0> failed with: <%1>").arg(query.lastQuery(), query.lastError().text());
		return false;
	}

	return true;
}

QVector<QVariantMap> Database::getUnsyncedEntries()
{
	QVector<QVariantMap> entries;

	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::GET_UNSYNCED_ENTRIES, query)) {
		return QVector<QVariantMap>();
	}
	if (!evaluateQueryExec(query)) {
		return QVector<QVariantMap>();
	}

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
	if (!evaluateQueryPrepare(SqlQueries::GET_RECENT_ENTRIES, query)) {
		return entries;
	}

	query.addBindValue(limit);
	if (!evaluateQueryExec(query)) {
		return entries;
	}

	while (query.next()) {
		QJsonObject entry;
		entry["id"] = query.value("id").toInt();
		entry["issue_id"] = query.value("issue_id").toInt();
		entry["hours"] = query.value("hours").toDouble();
		entry["comments"] = query.value("comments").toString();
		entry["spent_on"] = query.value("spent_on").toString();
		entry["sync_status"] = query.value("sync_status").toInt();
		entry["redmine_id"] = query.value("redmine_id").toInt();
		entry["project_id"] = query.value("project_id").toInt();
		entry["project_name"] = query.value("project_name").toString();
		entries.append(entry);
	}

	return entries;
}

QSet<int> Database::linkedRedmineIds()
{
	QSet<int> ids;
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::GET_LINKED_REDMINE_IDS, query) || !evaluateQueryExec(query)) {
		return ids;
	}
	while (query.next()) {
		ids.insert(query.value(0).toInt());
	}
	return ids;
}
}
