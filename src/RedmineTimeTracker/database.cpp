#include "database.h"
#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <qcontainerfwd.h>
#include <qcoreapplication.h>
#include <qjsonobject.h>
#include <qstringliteral.h>

Q_LOGGING_CATEGORY(lcDataBase, "qredmine.timeentry.database")

namespace QRedmineClient {
bool Database::initialize()
{
	static bool initialized = false;

	if (initialized) {
		return initialized;
	}

	QDir tmpDir;
	const auto homePath = QDir::homePath();
#ifdef Q_OS_WIN

	tmpDir = QDir(homePath + "/AppData/Local");
#endif
#ifdef Q_OS_LINUX
	tmpDir = QDir(homePath + "/.config/");
#endif

	const QString databaseDirName = QCoreApplication::instance()->applicationName();

	if (!tmpDir.exists(databaseDirName)) {
		tmpDir.mkdir(databaseDirName);
	}
	QDir databaseDir(tmpDir.absoluteFilePath(databaseDirName));

	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
	const auto databaseName = databaseDir.absoluteFilePath("timeentries.db");
	db.setDatabaseName(databaseName);

	if (!db.open()) {
		qWarning(lcDataBase) << QStringLiteral("Failed to open database: <%0>").arg(db.lastError().text());
		return false;
	}

	QSqlQuery query;

	if (!evaluateQueryPrepare(SqlQueries::ENABLE_WAL_MODE, query)) {
		return false;
	}

	if (!evaluateQueryExec(query)) {
		return false;
	}

	if (!evaluateQueryPrepare(SqlQueries::CREATE_TIME_ENTRY_TABLE, query)) {
		return false;
	}

	if (!evaluateQueryExec(query)) {
		return false;
	}

	if (!evaluateQueryPrepare(SqlQueries::CREATE_TIME_ENTRIES_CREATED_AT_INDEX, query)) {
		return false;
	}

	if (!evaluateQueryExec(query)) {
		return false;
	}

	if (!evaluateQueryPrepare(SqlQueries::CREATE_TIME_ENTRIES_SYNC_STATUS_INDEX, query)) {
		return false;
	}

	if (!evaluateQueryExec(query)) {
		return false;
	}

	if (!createIssuesDatabase()) {
		return false;
	}

	initialized = true;
	return true;
}

bool Database::createIssuesDatabase()
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::CREATE_ISSUES_TABLE, query)) {
		return false;
	}

	if (!evaluateQueryExec(query)) {
		return false;
	}

	return true;
}

bool Database::addIssue(int issueId, const QString& subject, int projectId, int statusId, const QString& description,
	const QDateTime& createdOn, const QDateTime& updatedOn)
{
	QSqlQuery query;
	// INSERT OR REPLACE
	if (!evaluateQueryPrepare(SqlQueries::REPLACE_ISSUE, query)) {
		return false;
	}

	query.bindValue(":id", issueId);
	query.bindValue(":subject", subject);
	query.bindValue(":project_id", projectId);
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

bool Database::updateTimeEntry(int id, double hours, const QString& comments, const QDateTime& spentOn)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::UPDATE_TIME_ENTRY, query)) {
		return false;
	}

	query.addBindValue(hours);
	query.addBindValue(comments);
	query.addBindValue(spentOn);
	query.addBindValue(QDateTime::currentDateTime());
	query.addBindValue(id);

	return evaluateQueryExec(query);
}

bool Database::updateTimeEntriesByRedmineId(
	int redmineId, double hours, const QString& comments, const QDateTime& spentOn)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::UPDATE_TIME_ENTRIES_BY_REDMINE_ID, query)) {
		return false;
	}

	query.addBindValue(hours);
	query.addBindValue(comments);
	query.addBindValue(spentOn);
	query.addBindValue(QDateTime::currentDateTime());
	query.addBindValue(redmineId);

	return evaluateQueryExec(query);
}

int Database::findUnsyncedMatch(int issueId, const QDate& spentOn, double hours, const QString& comments)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::FIND_UNSYNCED_MATCH, query)) {
		return -1;
	}

	query.addBindValue(issueId);
	query.addBindValue(spentOn.toString(Qt::ISODate));
	query.addBindValue(hours);
	query.addBindValue(comments);

	if (!evaluateQueryExec(query) || !query.next()) {
		return -1;
	}
	return query.value(0).toInt();
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
		entries.append(entry);
	}

	return entries;
}
}
