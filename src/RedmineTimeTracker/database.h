#include "stringhelper.h"
#include <QJsonArray>
#include <QSet>
#include <QString>
#include <QStringLiteral>
#include <qsqlquery.h>
#include <qstringliteral.h>
#include <qstringview.h>

namespace QRedmineClient {

namespace SqlQueries {
	STATIC_CONST_QSTRING(ENABLE_WAL_MODE, "PRAGMA journal_mode=WAL");
	STATIC_CONST_QSTRING(GET_SCHEMA_VERSION, "PRAGMA user_version");
	STATIC_CONST_QSTRING(SET_SCHEMA_VERSION, "PRAGMA user_version = %1");

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
	STATIC_CONST_QSTRING(CREATE_TIME_ENTRIES_CREATED_AT_INDEX,
		"CREATE INDEX IF NOT EXISTS idx_time_entries_created_at ON time_entries(created_at)");
	STATIC_CONST_QSTRING(CREATE_TIME_ENTRIES_SYNC_STATUS_INDEX,
		"CREATE INDEX IF NOT EXISTS idx_time_entries_sync_status ON time_entries(sync_status)");

	STATIC_CONST_QSTRING(ADD_TIME_ENTRY,
		"INSERT INTO time_entries "
		"(issue_id, hours, comments, spent_on, created_at, updated_at) "
		"VALUES (?, ?, ?, ?, ?, ?)");

	STATIC_CONST_QSTRING(GET_LINKED_REDMINE_IDS, "SELECT DISTINCT redmine_id FROM time_entries WHERE redmine_id > 0");

	STATIC_CONST_QSTRING(UPDATE_SYNC_STATE,
		"UPDATE time_entries "
		"SET sync_status = 1, redmine_id = ?, updated_at = ? "
		"WHERE id = ?");

	STATIC_CONST_QSTRING(GET_UNSYNCED_ENTRIES, "SELECT * FROM time_entries WHERE sync_status = 0");

	// Followed by the column selecting the entries to change ("id" or "redmine_id") and " = ?", see
	// Database::updateTimeEntries().
	STATIC_CONST_QSTRING(UPDATE_TIME_ENTRIES_WHERE,
		"UPDATE time_entries "
		"SET hours = ?, comments = ?, spent_on = ?, updated_at = ? "
		"WHERE ");

	// Local time entries, newest first, with the project of their issue where that issue is cached.
	STATIC_CONST_QSTRING(GET_RECENT_ENTRIES,
		"SELECT t.*, i.project_id AS project_id, i.project_name AS project_name "
		"FROM time_entries t LEFT JOIN issues i ON i.id = t.issue_id "
		"ORDER BY t.created_at DESC, t.id DESC "
		"LIMIT ?");

	// Issues of the latest local time entries (as many as GET_RECENT_ENTRIES shows) that aren't
	// cached with a project yet.
	STATIC_CONST_QSTRING(GET_ISSUE_IDS_WITHOUT_PROJECT,
		"SELECT DISTINCT t.issue_id "
		"FROM (SELECT issue_id FROM time_entries ORDER BY created_at DESC, id DESC LIMIT ?) t "
		"LEFT JOIN issues i ON i.id = t.issue_id "
		"WHERE i.project_name IS NULL OR i.project_name = ''");

	STATIC_CONST_QSTRING(CREATE_ISSUES_TABLE,
		"CREATE TABLE IF NOT EXISTS issues ("
		"id INTEGER PRIMARY KEY NOT NULL, "
		"subject TEXT NOT NULL, "
		"project_id INTEGER, "
		"project_name TEXT, "
		"status_id INTEGER NOT NULL, "
		"description TEXT, "
		"created_on DATETIME NOT NULL, "
		"updated_on DATETIME NOT NULL)");

	// project_name was added after the first release; databases created before get it added.
	STATIC_CONST_QSTRING(GET_ISSUES_COLUMNS, "PRAGMA table_info(issues)");
	STATIC_CONST_QSTRING(ADD_ISSUES_PROJECT_NAME_COLUMN, "ALTER TABLE issues ADD COLUMN project_name TEXT");

	STATIC_CONST_QSTRING(REPLACE_ISSUE,
		"REPLACE INTO issues ("
		"id, subject, project_id, project_name, status_id, description, created_on, updated_on) VALUES "
		"(:id,:subject, :project_id, :project_name, :status_id,:description,:created_on,:updated_on)");

	// Sorted by id; IssueModel groups them by project, which needs a locale aware comparison.
	STATIC_CONST_QSTRING(GET_ISSUES,
		"SELECT id, subject, project_id, project_name, status_id, description, created_on, updated_on FROM issues "
		"ORDER BY id");
}

class Database {
public:
	/*!
	 * \brief initialize Opens the database and brings its schema up to date (see migrate()). Does
	 * nothing if it's open already.
	 * \param path Database file, or ":memory:"; defaultPath() if empty, where a database from the
	 * location used before is moved to first
	 */
	static bool initialize(const QString& path = QString());

	//! Closes the database, so initialize() can open another one (e.g. between tests).
	static void close();

	//! The database file in the platform's application data directory.
	static QString defaultPath();

	//! The schema version (SQLite's user_version): how many migrations have run.
	static int schemaVersion();

	/*!
	 * \brief addTimeEntry Adds a local, not yet synced time entry
	 * \return The new entry's local id, or -1 on failure
	 */
	static int addTimeEntry(
		int issueId, double hours, const QString& comments, const QDateTime& spentOn = QDateTime::currentDateTime());

	/*!
	 * \brief updateTimeEntry Changes the local time entry with the given local id
	 * \return The number of changed entries (0 or 1), or -1 on failure
	 */
	static int updateTimeEntry(int id, double hours, const QString& comments, const QDateTime& spentOn);

	/*!
	 * \brief updateTimeEntriesByRedmineId Changes the local time entries linked to the given Redmine
	 * time entry (see updateSyncStatus()), if any
	 * \return The number of changed entries, or -1 on failure
	 */
	static int updateTimeEntriesByRedmineId(
		int redmineId, double hours, const QString& comments, const QDateTime& spentOn);

	static QVector<QVariantMap> getUnsyncedEntries();

	static QJsonArray getRecentEntries(int limit = 50);

	/*!
	 * \brief linkedRedmineIds Redmine time entries some local entry is linked to (see updateSyncStatus())
	 */
	static QSet<int> linkedRedmineIds();

	static bool addIssue(int issueId, const QString& subject, int projectId, const QString& projectName, int statusId,
		const QString& description, const QDateTime& createdOn, const QDateTime& updatedOn);

	/*!
	 * \brief issueIdsWithoutProject Issues of the latest local time entries that aren't cached with
	 * their project yet, so it can't be shown for those entries
	 * \param limit How many of the latest entries to check, as for getRecentEntries()
	 */
	static QList<int> issueIdsWithoutProject(int limit = 50);

	static QSqlQuery getIssues();

public slots:
	static bool updateSyncStatus(int id, int redmineId);

private:
	//! Runs the migrations the database hasn't had yet, each in a transaction.
	static bool migrate();
	static const QList<bool (*)()>& migrations();
	static bool migrateToVersion1();
	static bool execute(const QString& sql);

	static bool evaluateQueryPrepare(const QStringView& sqlCommand, QSqlQuery& query);
	static bool evaluateQueryExec(QSqlQuery& query);

	//! Shared by updateTimeEntry() and updateTimeEntriesByRedmineId(); keyColumn selects the entries.
	static int updateTimeEntries(
		const QString& keyColumn, int key, double hours, const QString& comments, const QDateTime& spentOn);
};
}
