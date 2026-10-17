#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace QRedmineClient {
/*!
 * \brief The TimeTracker class
 */
class TimeTracker : public QObject {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(bool isTracking READ isTracking NOTIFY trackingChanged)
	Q_PROPERTY(bool isPaused READ isPaused NOTIFY pausedChanged)
	Q_PROPERTY(int currentIssueId READ currentIssueId NOTIFY currentIssueChanged)
	Q_PROPERTY(QString elapsedTime READ elapsedTime NOTIFY elapsedTimeChanged)
	Q_PROPERTY(QVariantList recentEntries READ recentEntries NOTIFY recentEntriesChanged)
	Q_PROPERTY(QVariantList entriesFromId READ entriesFromId WRITE setEntriesFromId NOTIFY entriesFromIdChanged FINAL)
public:
	/*!
	 * \brief TimeTracker
	 * \param parent
	 */
	explicit TimeTracker(QObject* parent = nullptr);

	/*!
	 * \brief isTracking
	 * \return
	 */
	bool isTracking() const;

	/*!
	 * \brief isPaused
	 * \return
	 */
	bool isPaused() const;

	/*!
	 * \brief currentIssueId
	 * \return
	 */
	int currentIssueId() const;

	/*!
	 * \brief elapsedTime
	 * \return
	 */
	QString elapsedTime() const;

	/*!
	 * \brief recentEntries
	 * \return
	 */
	QVariantList recentEntries() const;

	/*!
	 * \brief getElapsedHours
	 * \return
	 */
	Q_INVOKABLE double getElapsedHours() const;

	QVariantList entriesFromId() const;
	void setEntriesFromId(const QVariantList& newEntriesFromId);

	/*!
	 * \brief updateRecentEntry Changes a local entry that isn't linked to a Redmine time entry
	 * \param localId The entry's "id" in recentEntries
	 * \param changes "hours" (decimal hours), "comments" and "spent_on" ("yyyy-MM-dd")
	 */
	Q_INVOKABLE void updateRecentEntry(int localId, const QVariantMap& changes);

	/*!
	 * \brief issueIdsWithoutProject Issues of recent entries whose project isn't known yet because
	 * the issue isn't cached; fetching them (RedmineClient::getIssue) fills it in.
	 */
	Q_INVOKABLE QVariantList issueIdsWithoutProject() const;

	/*!
	 * \brief unlinkedEntriesRange Days spanned by the recent entries of the last month that aren't
	 * linked to a Redmine time entry, e.g. entries saved before linking existed or whose link was missed
	 * \return {"from", "to"} as "yyyy-MM-dd" for RedmineClient::getOwnTimeEntries(), or an empty
	 * map if all recent entries are linked
	 */
	Q_INVOKABLE QVariantMap unlinkedEntriesRange() const;

	/*!
	 * \brief stageRedmineUpdate Remembers changes sent to Redmine for one of its time entries, to be
	 * applied locally by applyRedmineUpdate() once Redmine confirms them, so the local data never
	 * shows changes Redmine rejected
	 * \param redmineId Redmine's id of the time entry
	 * \param changes As for updateRecentEntry(), plus optional "activity_id" and "activity_name"
	 * \return Tag to pass to RedmineClient::updateTimeEntry(), which hands it back to
	 * applyRedmineUpdate() or discardRedmineUpdate()
	 */
	Q_INVOKABLE int stageRedmineUpdate(int redmineId, const QVariantMap& changes);

public slots:
	/*!
	 * \brief startTracking
	 * \param issueId
	 */
	void startTracking(int issueId);

	/*!
	 * \brief stopTracking
	 */
	void stopTracking();

	/*!
	 * \brief pauseTracking
	 */
	void pauseTracking();

	/*!
	 * \brief resumeTracking
	 */
	void resumeTracking();

	/*!
	 * \brief addRecentEntry
	 * \param entry
	 * \return The new entry's local id, to pass to RedmineClient::createTimeEntry() as requestTag
	 * so linkCreatedTimeEntry() can link it; -1 if it couldn't be saved
	 */
	int addRecentEntry(const QVariantMap& entry);

	/*!
	 * \brief addEntriesFromId
	 * \param entries
	 */
	void addEntriesFromId(const QJsonArray& entries);

	/*!
	 * \brief addIssueToDatabase Adds an issue to the database using the received JSON object
	 * \param issue
	 */
	void addIssueToDatabase(const QJsonObject& issue);

	/*!
	 * \brief applyRedmineUpdate Applies the changes staged for a Redmine time entry (see
	 * stageRedmineUpdate()) to the local entries linked to it and to entriesFromId. Connected to
	 * RedmineClient::timeEntryUpdated.
	 * \param tag As returned by stageRedmineUpdate()
	 */
	void applyRedmineUpdate(int redmineId, int tag);

	/*!
	 * \brief discardRedmineUpdate Forgets changes Redmine rejected (see stageRedmineUpdate()).
	 * Connected to RedmineClient::timeEntryUpdateFailed.
	 * \param tag As returned by stageRedmineUpdate()
	 */
	void discardRedmineUpdate(int redmineId, int tag);

	/*!
	 * \brief linkCreatedTimeEntry Links the local entry a time entry just created on Redmine was
	 * made from to that time entry, so it can be edited on Redmine later. Connected to
	 * RedmineClient::timeEntryCreated.
	 * \param localId The local entry's id, as returned by addRecentEntry(); ignored if not positive
	 */
	void linkCreatedTimeEntry(const QJsonObject& timeEntry, int localId);

	/*!
	 * \brief linkExistingTimeEntries Links recent local entries that aren't linked yet to the
	 * user's time entries on Redmine they were submitted as: same issue, day and comment, hours
	 * equal up to Redmine's rounding. Entries without such a match stay unlinked.
	 * Connected to RedmineClient::ownTimeEntriesReceived.
	 * \param timeEntries The user's time entries on Redmine for unlinkedEntriesRange()
	 */
	void linkExistingTimeEntries(const QJsonArray& timeEntries);

signals:
	/*!
	 * \brief trackingChanged
	 */
	void trackingChanged();

	/*!
	 * \brief pausedChanged
	 */
	void pausedChanged();

	/*!
	 * \brief currentIssueChanged
	 */
	void currentIssueChanged();

	/*!
	 * \brief elapsedTimeChanged
	 */
	void elapsedTimeChanged();

	/*!
	 * \brief recentEntriesChanged
	 */
	void recentEntriesChanged();

	void entriesFromIdChanged();

	void issueAddedOrUpdated();

private slots:
	/*!
	 * \brief updateElapsedTime
	 */
	void updateElapsedTime();

private:
	/*!
	 * \brief loadRecentEntries
	 */
	void loadRecentEntries();

	QTimer mTimer;
	int mCurrentIssueId;
	bool mIsPaused;
	QDateTime mStartTime;
	qint64 mTotalElapsedTime;
	QVariantList mRecentEntries;
	QVariantList mEntriesFromId;
	//! Changes sent to Redmine but not confirmed yet, by the tag stageRedmineUpdate() returned. Kept
	//! per request rather than per time entry, so a confirmation applies exactly the changes it
	//! confirms even while another update of the same entry is under way.
	QHash<int, QVariantMap> mPendingRedmineUpdates;
	int mLastRedmineUpdateTag = 0;
};
}
