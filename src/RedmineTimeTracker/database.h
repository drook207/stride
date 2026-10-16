#include "stringhelper.h"
#include <QJsonArray>
#include <QString>
#include <QStringLiteral>
#include <qsqlquery.h>
#include <qstringliteral.h>
#include <qstringview.h>

namespace QRedmineClient {

namespace SqlQueries {
	STATIC_CONST_QSTRING(CREATE_TIME_ENTRY_TABLE,
		"CREATE TABLE IF NOT EXISTS time_entries ("
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
	STATIC_CONST_QSTRING(ADD_TIME_ENTRY,
		"INSERT INTO time_entries "
		"(issue_id, hours, comments, spent_on, created_at, updated_at) "
		"VALUES (?, ?, ?, ?, ?, ?)");

	STATIC_CONST_QSTRING(UPDATE_SYNC_STATE,
		"UPDATE time_entries "
		"SET sync_status = 1, redmine_id = ?, updated_at = ? "
		"WHERE id = ?");

	STATIC_CONST_QSTRING(GET_UNSYNCED_ENTRIES, "SELECT * FROM time_entries WHERE sync_status = 0");

	STATIC_CONST_QSTRING(GET_RECENT_ENTRIES,
		"SELECT * FROM time_entries "
		"ORDER BY created_at DESC "
		"LIMIT ?");

	STATIC_CONST_QSTRING(CREATE_ISSUES_TABLE,
		"CREATE TABLE IF NOT EXISTS issues ("
		"id INTEGER PRIMARY KEY NOT NULL, "
		"subject TEXT NOT NULL, "
		"project_id INTEGER, "
		"status_id INTEGER NOT NULL, "
		"description TEXT, "
		"created_on DATETIME NOT NULL, "
		"updated_on DATETIME NOT NULL)");

	STATIC_CONST_QSTRING(REPLACE_ISSUE,
		"REPLACE INTO issues ("
		"id, subject, project_id, status_id, description, created_on, updated_on) VALUES "
		"(:id,:subject, :project_id,:status_id,:description,:created_on,:updated_on)");

	STATIC_CONST_QSTRING(GET_ISSUES, "SELECT * FROM issues");
}

class Database {
public:
	static bool initialize();

	static bool addTimeEntry(
		int issueId, double hours, const QString& comments, const QDateTime& spentOn = QDateTime::currentDateTime());

	static QVector<QVariantMap> getUnsyncedEntries();

	static QJsonArray getRecentEntries(int limit = 50);

	static bool createIssuesDatabase();

	static bool addIssue(int issueId, const QString& subject, int projectId, int statusId, const QString& description,
		const QDateTime& createdOn, const QDateTime& updatedOn);

	static QSqlQuery getIssues();

public slots:
	static bool updateSyncStatus(int id, int redmineId);

private:
	static bool evaluateQueryPrepare(const QStringView& sqlCommand, QSqlQuery& query);
	static bool evaluateQueryExec(QSqlQuery& query);
};
}
