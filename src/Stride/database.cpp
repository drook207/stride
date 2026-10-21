#include "database.h"
#include "appdatapath.h"
#include "logging.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
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

namespace {
//! The error of the query that failed last, for a migration to report it: after rollback() the database's
//! own lastError() no longer tells
thread_local QString sLastQueryError;
}

namespace Stride {

static bool sInitialized = false;

static const QString DATABASE_FILE_NAME = QStringLiteral("timeentries.db");

QString Database::defaultPath()
{
	return QDir(QRedmineClient::Utils::appDataPath(QStandardPaths::AppDataLocation))
		.absoluteFilePath(DATABASE_FILE_NAME);
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

// Moves a database from its legacy location to path, unless there already is one at path.
static void moveLegacyDatabase(const QString& path)
{
	const QString legacy = legacyPath();
	if (legacy == path || !QFile::exists(legacy) || QFile::exists(path)) {
		return;
	}
	if (Database::moveDatabaseFiles(legacy, path)) {
		QDir().rmdir(QFileInfo(legacy).absolutePath()); // only if nothing else is left in it
		qInfo(lcDataBase) << "Moved the database from" << legacy << "to" << path;
	} else {
		qWarning(lcDataBase) << "Could not move the database from" << legacy << "to" << path;
	}
}

bool Database::moveDatabaseFiles(const QString& from, const QString& to)
{
	// Write what's in the write-ahead log into the database file first, so normally there's only
	// that one file left to move.
	{
		QSqlDatabase legacy = QSqlDatabase::addDatabase("QSQLITE", QStringLiteral("legacy-move"));
		legacy.setDatabaseName(from);
		if (legacy.open()) {
			QSqlQuery(legacy).exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
			legacy.close();
		}
	}
	QSqlDatabase::removeDatabase(QStringLiteral("legacy-move"));

	// Copy everything before removing anything, so a failure leaves the old database complete
	// (and the move is tried again on the next start) rather than split across both places.
	const QStringList suffixes { QString(), QStringLiteral("-wal"), QStringLiteral("-shm") };
	QStringList copied;
	for (const QString& suffix : suffixes) {
		if (!QFile::exists(from + suffix)) {
			continue;
		}
		QFile::remove(to + suffix); // a leftover from an earlier, failed attempt
		if (!QFile::copy(from + suffix, to + suffix)) {
			for (const QString& done : std::as_const(copied)) {
				QFile::remove(to + done);
			}
			return false;
		}
		copied << suffix;
	}
	for (const QString& suffix : std::as_const(copied)) {
		QFile::remove(from + suffix);
	}
	return true;
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
	static const QList<bool (*)()> list { &Database::migrateToVersion1, &Database::migrateToVersion2,
		&Database::migrateToVersion3, &Database::migrateToVersion4, &Database::migrateToVersion5,
		&Database::migrateToVersion6 };
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
		sLastQueryError.clear();
		// PRAGMA doesn't take bound values; the version is an int, so formatting it in is safe.
		if (!steps[next]() || !execute(SqlQueries::SET_SCHEMA_VERSION.arg(next + 1))) {
			// Before rollback(), which would be the last one
			const QString error = sLastQueryError;
			db.rollback();
			QR_WARNING(lcDataBase, QStringLiteral("Database migration to version %1 failed").arg(next + 1),
				{ { "from", next }, { "to", next + 1 }, { "error", error } });
			return false;
		}
		db.commit();
		QR_INFO(lcDataBase, QStringLiteral("Database migrated to schema version %1").arg(next + 1),
			{ { "from", next }, { "to", next + 1 } });
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

bool Database::migrateToVersion2()
{
	return execute(SqlQueries::ADD_ACTIVITY_ID_COLUMN) && execute(SqlQueries::ADD_CUSTOM_FIELDS_COLUMN) &&
		execute(SqlQueries::ADD_SYNC_ERROR_COLUMN) && execute(SqlQueries::MARK_UNLINKED_UNCONFIRMED);
}

bool Database::migrateToVersion3() { return execute(SqlQueries::CREATE_TRACKING_STATE_TABLE); }

bool Database::migrateToVersion4() { return execute(SqlQueries::CREATE_FAVORITE_ISSUES_TABLE); }

bool Database::migrateToVersion5() { return execute(SqlQueries::ADD_ISSUE_STATUS_ID_COLUMN); }

bool Database::migrateToVersion6() { return execute(SqlQueries::ADD_TRACKING_STARTED_AT_COLUMN); }

bool Database::clearIssueStatus(int id)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::CLEAR_ISSUE_STATUS_ID, query)) {
		return false;
	}
	query.addBindValue(id);
	return evaluateQueryExec(query);
}

bool Database::addFavoriteIssue(int issueId)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::ADD_FAVORITE_ISSUE, query)) {
		return false;
	}
	query.addBindValue(issueId);
	query.addBindValue(QDateTime::currentDateTime());
	return evaluateQueryExec(query);
}

bool Database::removeFavoriteIssue(int issueId)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::REMOVE_FAVORITE_ISSUE, query)) {
		return false;
	}
	query.addBindValue(issueId);
	return evaluateQueryExec(query);
}

QVariantList Database::getFavoriteIssues()
{
	QVariantList favorites;
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::GET_FAVORITE_ISSUES, query) || !evaluateQueryExec(query)) {
		return favorites;
	}
	while (query.next()) {
		favorites.append(QVariantMap { { "issue_id", query.value("issue_id").toInt() },
			{ "subject", query.value("subject").toString() },
			{ "project_name", query.value("project_name").toString() } });
	}
	return favorites;
}

bool Database::saveTrackingState(const QVariantMap& state)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::SAVE_TRACKING_STATE, query)) {
		return false;
	}
	const QDateTime startedAt = state["started_at"].toDateTime();
	query.addBindValue(state["issue_id"].toInt());
	query.addBindValue(startedAt.isValid() ? QVariant(startedAt) : QVariant(QMetaType::fromType<QDateTime>()));
	query.addBindValue(state["elapsed_seconds"].toLongLong());
	query.addBindValue(state["comment"].toString());
	query.addBindValue(state["activity_id"].toInt());
	query.addBindValue(QDateTime::currentDateTime());
	const QDateTime trackingStartedAt = state["tracking_started_at"].toDateTime();
	query.addBindValue(
		trackingStartedAt.isValid() ? QVariant(trackingStartedAt) : QVariant(QMetaType::fromType<QDateTime>()));
	return evaluateQueryExec(query);
}

QVariantMap Database::loadTrackingState()
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::GET_TRACKING_STATE, query) || !evaluateQueryExec(query) || !query.next()) {
		return {};
	}
	return { { "issue_id", query.value("issue_id").toInt() },
		{ "started_at", query.value("started_at").isNull() ? QDateTime() : query.value("started_at").toDateTime() },
		{ "elapsed_seconds", query.value("elapsed_seconds").toLongLong() },
		{ "comment", query.value("comment").toString() }, { "activity_id", query.value("activity_id").toInt() },
		{ "last_seen", query.value("last_seen").toDateTime() },
		{ "tracking_started_at",
			query.value("tracking_started_at").isNull() ? QDateTime()
														: query.value("tracking_started_at").toDateTime() } };
}

bool Database::clearTrackingState() { return execute(SqlQueries::CLEAR_TRACKING_STATE); }

bool Database::touchTrackingState()
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::TOUCH_TRACKING_STATE, query)) {
		return false;
	}
	query.addBindValue(QDateTime::currentDateTime());
	return evaluateQueryExec(query);
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

// custom_fields is stored as a JSON array; NULL when there are none.
static QVariant customFieldsColumn(const QVariantList& customFields)
{
	if (customFields.isEmpty()) {
		return QVariant(QMetaType::fromType<QString>());
	}
	return QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(customFields)).toJson(QJsonDocument::Compact));
}

int Database::addTimeEntry(int issueId, double hours, const QString& comments, const QDateTime& spentOn, int activityId,
	const QVariantList& customFields, int issueStatusId)
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
	query.addBindValue(activityId > 0 ? QVariant(activityId) : QVariant(QMetaType::fromType<int>()));
	query.addBindValue(customFieldsColumn(customFields));
	query.addBindValue(issueStatusId > 0 ? QVariant(issueStatusId) : QVariant(QMetaType::fromType<int>()));
	query.addBindValue(now);
	query.addBindValue(now);

	if (!evaluateQueryExec(query)) {
		return -1;
	}
	return query.lastInsertId().toInt();
}

int Database::updateTimeEntry(int id, double hours, const QString& comments, const QDateTime& spentOn, int activityId,
	const QVariant& customFields)
{
	return updateTimeEntries(QStringLiteral("id"), id, hours, comments, spentOn, activityId, customFields);
}

int Database::updateTimeEntriesByRedmineId(int redmineId, double hours, const QString& comments,
	const QDateTime& spentOn, int activityId, const QVariant& customFields)
{
	return updateTimeEntries(
		QStringLiteral("redmine_id"), redmineId, hours, comments, spentOn, activityId, customFields);
}

int Database::updateTimeEntries(const QString& keyColumn, int key, double hours, const QString& comments,
	const QDateTime& spentOn, int activityId, const QVariant& customFields)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::UPDATE_TIME_ENTRIES_WHERE + keyColumn + QStringLiteral(" = ?"), query)) {
		return -1;
	}

	query.addBindValue(hours);
	query.addBindValue(comments);
	query.addBindValue(spentOn);
	// NULL keeps the current value (COALESCE in the query).
	query.addBindValue(activityId > 0 ? QVariant(activityId) : QVariant(QMetaType::fromType<int>()));
	query.addBindValue(customFields.isValid()
			? QVariant(QString::fromUtf8(
				  QJsonDocument(QJsonArray::fromVariantList(customFields.toList())).toJson(QJsonDocument::Compact)))
			: QVariant(QMetaType::fromType<QString>()));
	query.addBindValue(QDateTime::currentDateTime());
	query.addBindValue(key);

	if (!evaluateQueryExec(query)) {
		return -1;
	}
	return query.numRowsAffected();
}

bool Database::setSyncStatus(int id, SyncStatus status, const QString& error)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::SET_SYNC_STATUS, query)) {
		return false;
	}
	query.addBindValue(static_cast<int>(status));
	query.addBindValue(error.isEmpty() ? QVariant(QMetaType::fromType<QString>()) : QVariant(error));
	query.addBindValue(QDateTime::currentDateTime());
	query.addBindValue(id);
	return evaluateQueryExec(query);
}

static int deleteWhere(const QString& sql, int key)
{
	QSqlQuery query;
	if (!query.prepare(sql)) {
		return -1;
	}
	query.addBindValue(key);
	return query.exec() ? query.numRowsAffected() : -1;
}

int Database::deleteTimeEntry(int id) { return deleteWhere(SqlQueries::DELETE_TIME_ENTRY, id); }

int Database::removeIssue(int issueId) { return deleteWhere(SqlQueries::DELETE_ISSUE, issueId); }

int Database::deleteTimeEntriesByRedmineId(int redmineId)
{
	return deleteWhere(SqlQueries::DELETE_TIME_ENTRIES_BY_REDMINE_ID, redmineId);
}

QJsonArray Database::getPendingEntries()
{
	QJsonArray entries;
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::GET_PENDING_ENTRIES, query) || !evaluateQueryExec(query)) {
		return entries;
	}
	while (query.next()) {
		entries.append(entryFromQuery(query));
	}
	return entries;
}

QJsonObject Database::getTimeEntry(int id)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::GET_TIME_ENTRY, query)) {
		return {};
	}
	query.addBindValue(id);
	if (!evaluateQueryExec(query) || !query.next()) {
		return {};
	}
	return entryFromQuery(query);
}

QJsonObject Database::entryFromQuery(const QSqlQuery& query)
{
	QJsonObject entry;
	entry["id"] = query.value("id").toInt();
	entry["issue_id"] = query.value("issue_id").toInt();
	entry["hours"] = query.value("hours").toDouble();
	entry["comments"] = query.value("comments").toString();
	entry["spent_on"] = query.value("spent_on").toString();
	entry["sync_status"] = query.value("sync_status").toInt();
	entry["sync_error"] = query.value("sync_error").toString();
	entry["redmine_id"] = query.value("redmine_id").toInt();
	entry["activity_id"] = query.value("activity_id").toInt();
	entry["custom_fields"] = QJsonDocument::fromJson(query.value("custom_fields").toString().toUtf8()).array();
	entry["issue_status_id"] = query.value("issue_status_id").toInt();
	return entry;
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
		sLastQueryError = query.lastError().text();
		qWarning(lcDataBase()) << QStringLiteral("SQL Query prepare <%0> failed with: <%1>")
									  .arg(query.lastQuery(), query.lastError().text());
		return false;
	}

	return true;
}

bool Database::evaluateQueryExec(QSqlQuery& query)
{
	if (!query.exec()) {
		sLastQueryError = query.lastError().text();
		qWarning(lcDataBase())
			<< QStringLiteral("SQL Query exec <%0> failed with: <%1>").arg(query.lastQuery(), query.lastError().text());
		return false;
	}

	return true;
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
		QJsonObject entry = entryFromQuery(query);
		entry["project_id"] = query.value("project_id").toInt();
		entry["project_name"] = query.value("project_name").toString();
		entries.append(entry);
	}

	return entries;
}

QJsonArray Database::getEntriesBetween(const QDate& from, const QDate& to)
{
	QJsonArray entries;
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::GET_ENTRIES_BETWEEN, query)) {
		return entries;
	}
	query.addBindValue(from.toString(Qt::ISODate));
	query.addBindValue(to.toString(Qt::ISODate));
	if (!evaluateQueryExec(query)) {
		return entries;
	}
	while (query.next()) {
		QJsonObject entry = entryFromQuery(query);
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
