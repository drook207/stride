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

	const QString databaseDirName = QCoreApplication::instance()->applicationName();

	if (!tmpDir.exists(databaseDirName)) {
		tmpDir.mkdir(databaseDirName);
	}
	QDir databaseDir(tmpDir.absoluteFilePath(databaseDirName));

	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
	const auto databaseName = databaseDir.absoluteFilePath("timeentries.db");
	db.setDatabaseName(databaseName);

	if (!db.open()) {
		qWarning(lcDataBase) << QT_TR_NOOP(QStringLiteral("Failed to open database: <%0>")).arg(db.lastError().text());
		return false;
	}

	QSqlQuery query;

	if (!evaluateQueryPrepare(SqlQueries::CREATE_TIME_ENTRY_ACTIVITIES_TABLE, query)) {
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
	return true;
}

bool Database::addTimeEntry(int issueId, double hours, const QString& comments, const QDateTime& spentOn)
{
	QSqlQuery query;
	if (!evaluateQueryPrepare(SqlQueries::ADD_TIME_ENTRY, query)) {
		return false;
	}

	QDateTime now = QDateTime::currentDateTime();
	query.addBindValue(issueId);
	query.addBindValue(hours);
	query.addBindValue(comments);
	query.addBindValue(spentOn);
	query.addBindValue(now);
	query.addBindValue(now);

	if (!evaluateQueryExec(query)) {
		return false;
	}
	return true;
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

bool Database::addTimeEntryActivities(const QJsonArray& activities)
{
	QSqlQuery query;
	bool status = false;
	if (!evaluateQueryPrepare(SqlQueries::ADD_TIME_ENTRY_ACTIVITIE, query)) {
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
		status &= evaluateQueryExec(query);
	}
	return status;
}

bool Database::evaluateQueryPrepare(const QStringView& sqlCommand, QSqlQuery& query)
{
	if (sqlCommand.isEmpty()) {
		qWarning(lcDataBase()) << QT_TR_NOOP(QStringLiteral("Empty sql command"));
		return false;
	}
	if (!query.prepare(sqlCommand.toString())) {
		qWarning(lcDataBase()) << QT_TR_NOOP(QStringLiteral("SQL Query prepare <%0> failed with: <%1>")
												 .arg(query.lastQuery(), query.lastError().text()));
		return false;
	}
	return true;
}

bool Database::evaluateQueryExec(QSqlQuery& query)
{
	if (!query.isValid()) {
		qWarning(lcDataBase()) << QT_TR_NOOP(QStringLiteral("Invalid SQL Query"));
		return false;
	}
	if (!query.exec()) {
		qWarning(lcDataBase()) << QT_TR_NOOP(QStringLiteral("SQL Query prepare <%0> failed with: <%1>")
												 .arg(query.lastQuery(), query.lastError().text()));
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
