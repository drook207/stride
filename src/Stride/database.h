#pragma once

#include "stringhelper.h"
#include <QJsonArray>
#include <QSet>
#include <QString>
#include <QStringLiteral>
#include <qsqlquery.h>
#include <qstringliteral.h>
#include <qstringview.h>

namespace Stride {

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
		"(issue_id, hours, comments, spent_on, activity_id, custom_fields, issue_status_id, created_at, updated_at) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");

	// Schema version 5: the status to set the issue to once the entry is on Redmine.
	STATIC_CONST_QSTRING(ADD_ISSUE_STATUS_ID_COLUMN, "ALTER TABLE time_entries ADD COLUMN issue_status_id INTEGER");
	STATIC_CONST_QSTRING(CLEAR_ISSUE_STATUS_ID, "UPDATE time_entries SET issue_status_id = NULL WHERE id = ?");

	STATIC_CONST_QSTRING(GET_LINKED_REDMINE_IDS, "SELECT DISTINCT redmine_id FROM time_entries WHERE redmine_id > 0");

	STATIC_CONST_QSTRING(UPDATE_SYNC_STATE,
		"UPDATE time_entries "
		"SET sync_status = 1, redmine_id = ?, sync_error = NULL, updated_at = ? "
		"WHERE id = ?");

	STATIC_CONST_QSTRING(
		SET_SYNC_STATUS, "UPDATE time_entries SET sync_status = ?, sync_error = ?, updated_at = ? WHERE id = ?");

	// Oldest first, so they reach Redmine in the order they were saved.
	STATIC_CONST_QSTRING(GET_PENDING_ENTRIES,
		"SELECT * FROM time_entries WHERE sync_status = 0 AND (redmine_id IS NULL OR redmine_id = 0) "
		"ORDER BY created_at, id");
	STATIC_CONST_QSTRING(GET_TIME_ENTRY, "SELECT * FROM time_entries WHERE id = ?");
	STATIC_CONST_QSTRING(DELETE_TIME_ENTRY, "DELETE FROM time_entries WHERE id = ?");
	STATIC_CONST_QSTRING(DELETE_ISSUE, "DELETE FROM issues WHERE id = ?");
	STATIC_CONST_QSTRING(DELETE_TIME_ENTRIES_BY_REDMINE_ID, "DELETE FROM time_entries WHERE redmine_id = ?");

	// Schema version 2: what's needed to send an entry (again) later, and why Redmine rejected it.
	// Unlinked entries from before can't be told apart from ones Redmine already has, so they
	// aren't sent automatically (SyncStatus::Unconfirmed).
	STATIC_CONST_QSTRING(ADD_ACTIVITY_ID_COLUMN, "ALTER TABLE time_entries ADD COLUMN activity_id INTEGER");
	STATIC_CONST_QSTRING(ADD_CUSTOM_FIELDS_COLUMN, "ALTER TABLE time_entries ADD COLUMN custom_fields TEXT");
	STATIC_CONST_QSTRING(ADD_SYNC_ERROR_COLUMN, "ALTER TABLE time_entries ADD COLUMN sync_error TEXT");
	// Schema version 3: the running (or paused) tracker, so it survives closing the app. One row at most.
	STATIC_CONST_QSTRING(CREATE_TRACKING_STATE_TABLE,
		"CREATE TABLE tracking_state ("
		"id INTEGER PRIMARY KEY CHECK (id = 1), "
		"issue_id INTEGER NOT NULL, "
		"started_at DATETIME, " // start of the current running stretch; NULL while paused
		"elapsed_seconds INTEGER NOT NULL, " // tracked before started_at
		"comment TEXT, "
		"activity_id INTEGER, "
		"last_seen DATETIME NOT NULL)"); // when the app last ran with this state
	STATIC_CONST_QSTRING(SAVE_TRACKING_STATE,
		"REPLACE INTO tracking_state (id, issue_id, started_at, elapsed_seconds, comment, activity_id, last_seen, "
		"tracking_started_at) VALUES (1, ?, ?, ?, ?, ?, ?, ?)");
	STATIC_CONST_QSTRING(GET_TRACKING_STATE, "SELECT * FROM tracking_state WHERE id = 1");
	STATIC_CONST_QSTRING(CLEAR_TRACKING_STATE, "DELETE FROM tracking_state");
	STATIC_CONST_QSTRING(TOUCH_TRACKING_STATE, "UPDATE tracking_state SET last_seen = ?");

	// Schema version 6: when the tracking began (TimeTracker::startedAt()); NULL for a tracker saved before.
	STATIC_CONST_QSTRING(
		ADD_TRACKING_STARTED_AT_COLUMN, "ALTER TABLE tracking_state ADD COLUMN tracking_started_at DATETIME");

	// Schema version 4: issues the user pinned for quick access.
	STATIC_CONST_QSTRING(CREATE_FAVORITE_ISSUES_TABLE,
		"CREATE TABLE favorite_issues (issue_id INTEGER PRIMARY KEY NOT NULL, created_at DATETIME NOT NULL)");
	STATIC_CONST_QSTRING(
		ADD_FAVORITE_ISSUE, "INSERT OR IGNORE INTO favorite_issues (issue_id, created_at) VALUES (?, ?)");
	STATIC_CONST_QSTRING(REMOVE_FAVORITE_ISSUE, "DELETE FROM favorite_issues WHERE issue_id = ?");
	// In the order they were pinned, with what's cached about each issue.
	STATIC_CONST_QSTRING(GET_FAVORITE_ISSUES,
		"SELECT f.issue_id, i.subject, i.project_name FROM favorite_issues f "
		"LEFT JOIN issues i ON i.id = f.issue_id ORDER BY f.created_at, f.issue_id");

	STATIC_CONST_QSTRING(MARK_UNLINKED_UNCONFIRMED,
		"UPDATE time_entries SET sync_status = 2 WHERE redmine_id IS NULL OR redmine_id = 0");

	// Followed by the column selecting the entries to change ("id" or "redmine_id") and " = ?", see
	// Database::updateTimeEntries().
	STATIC_CONST_QSTRING(UPDATE_TIME_ENTRIES_WHERE,
		"UPDATE time_entries "
		"SET hours = ?, comments = ?, spent_on = ?, activity_id = COALESCE(?, activity_id), "
		"custom_fields = COALESCE(?, custom_fields), updated_at = ? "
		"WHERE ");

	// Local time entries, newest first, with the project of their issue where that issue is cached.
	// Local time entries spent on the days from .. to, with their issue's project as for GET_RECENT_ENTRIES.
	STATIC_CONST_QSTRING(GET_ENTRIES_BETWEEN,
		"SELECT t.*, i.project_id AS project_id, i.project_name AS project_name "
		"FROM time_entries t LEFT JOIN issues i ON i.id = t.issue_id "
		"WHERE date(t.spent_on) BETWEEN ? AND ? "
		"ORDER BY t.spent_on, t.id");

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
	//! sync_status of a local time entry.
	enum SyncStatus {
		Pending = 0, // not on Redmine yet, sent when possible
		Synced = 1, // on Redmine, linked via redmine_id
		Unconfirmed = 2, // saved before sending was tracked: may or may not be on Redmine
		Rejected = 3, // Redmine refused it (sync_error says why); sent again once changed
	};

	/*!
	 * \brief initialize Opens the database and brings its schema up to date (see migrate()). Does
	 * nothing if it's open already; if it can't be opened or migrated, no connection is left open
	 * (the next call starts over).
	 * \param path Database file, or ":memory:"; defaultPath() if empty, where a database from the
	 * location used before is moved to first
	 */
	static bool initialize(const QString& path = QString());

	//! Closes the database, so initialize() can open another one (e.g. between tests).
	static void close();

	//! The database file in the platform's application data directory.
	static QString defaultPath();

	/*!
	 * \brief moveDatabaseFiles Moves a database file with its SQLite companions (-wal, -shm), after
	 * writing its write-ahead log into it. Either everything is moved, or nothing changes at from.
	 * \return Whether it was moved
	 */
	static bool moveDatabaseFiles(const QString& from, const QString& to);

	//! The schema version (SQLite's user_version): how many migrations have run.
	static int schemaVersion();

	/*!
	 * \brief addTimeEntry Adds a local time entry, pending to be sent to Redmine
	 * \param activityId Redmine activity to send it with; 0 for Redmine's default
	 * \param customFields Custom field values to send it with, as [{"id", "value"}]
	 * \param issueStatusId Status to set the issue to once the entry is on Redmine; 0 for none
	 * \return The new entry's local id, or -1 on failure
	 */
	static int addTimeEntry(int issueId, double hours, const QString& comments,
		const QDateTime& spentOn = QDateTime::currentDateTime(), int activityId = 0,
		const QVariantList& customFields = {}, int issueStatusId = 0);

	//! Forgets the status to set the entry's issue to, e.g. once it was set.
	static bool clearIssueStatus(int id);

	/*!
	 * \brief updateTimeEntry Changes the local time entry with the given local id
	 * \param activityId New activity; 0 keeps the current one
	 * \param customFields New custom field values ([{"id", "value"}], all of them); an invalid
	 * QVariant keeps the current ones
	 * \return The number of changed entries (0 or 1), or -1 on failure
	 */
	static int updateTimeEntry(int id, double hours, const QString& comments, const QDateTime& spentOn,
		int activityId = 0, const QVariant& customFields = {});

	/*!
	 * \brief updateTimeEntriesByRedmineId Changes the local time entries linked to the given Redmine
	 * time entry (see updateSyncStatus()), if any; activityId and customFields as for updateTimeEntry()
	 * \return The number of changed entries, or -1 on failure
	 */
	static int updateTimeEntriesByRedmineId(int redmineId, double hours, const QString& comments,
		const QDateTime& spentOn, int activityId = 0, const QVariant& customFields = {});

	/*!
	 * \brief getPendingEntries Entries to send to Redmine (SyncStatus::Pending), oldest first, with
	 * the fields getRecentEntries() lists
	 */
	static QJsonArray getPendingEntries();

	/*!
	 * \brief getTimeEntry One local entry with the fields getRecentEntries() lists; empty if unknown
	 */
	static QJsonObject getTimeEntry(int id);

	/*!
	 * \brief setSyncStatus Sets an entry's SyncStatus and why it was rejected (empty otherwise)
	 */
	static bool setSyncStatus(int id, SyncStatus status, const QString& error = QString());

	//! Removes an issue from the cache; returns the number removed (0 or 1), or -1 on failure.
	static int removeIssue(int issueId);

	//! Deletes a local entry; returns the number deleted (0 or 1), or -1 on failure.
	static int deleteTimeEntry(int id);
	//! Deletes the local entries linked to a Redmine time entry; returns the number deleted, or -1.
	static int deleteTimeEntriesByRedmineId(int redmineId);

	/*!
	 * \brief saveTrackingState Remembers the tracker: "issue_id", "started_at" (QDateTime, invalid
	 * while paused), "elapsed_seconds", "comment", "activity_id", "tracking_started_at" (QDateTime, when
	 * the tracking began; may be invalid); last_seen is set to now
	 */
	static bool saveTrackingState(const QVariantMap& state);
	//! The remembered tracker (fields as for saveTrackingState(), plus "last_seen"); empty if none.
	static QVariantMap loadTrackingState();
	static bool clearTrackingState();
	//! Records that the app is still running with the remembered tracker.
	static bool touchTrackingState();

	static bool addFavoriteIssue(int issueId);
	static bool removeFavoriteIssue(int issueId);
	//! Pinned issues, oldest first: "issue_id", "subject" and "project_name" ("" while not cached).
	static QVariantList getFavoriteIssues();

	static QJsonArray getRecentEntries(int limit = 50);

	//! Local entries spent on the days from .. to (both included), fields as for getRecentEntries().
	static QJsonArray getEntriesBetween(const QDate& from, const QDate& to);

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
	static bool migrateToVersion2();
	static bool migrateToVersion3();
	static bool migrateToVersion4();
	static bool migrateToVersion5();
	static bool migrateToVersion6();
	//! An entry row as a JSON object (see getRecentEntries()).
	static QJsonObject entryFromQuery(const QSqlQuery& query);
	static bool execute(const QString& sql);

	static bool evaluateQueryPrepare(const QStringView& sqlCommand, QSqlQuery& query);
	static bool evaluateQueryExec(QSqlQuery& query);

	//! Shared by updateTimeEntry() and updateTimeEntriesByRedmineId(); keyColumn selects the entries.
	static int updateTimeEntries(const QString& keyColumn, int key, double hours, const QString& comments,
		const QDateTime& spentOn, int activityId, const QVariant& customFields);
};
}
