#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSet>
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
	//! Comment and activity entered for the tracked time; kept with the tracker across restarts.
	Q_PROPERTY(
		QString trackingComment READ trackingComment WRITE setTrackingComment NOTIFY trackingDetailsChanged FINAL)
	Q_PROPERTY(
		int trackingActivityId READ trackingActivityId WRITE setTrackingActivityId NOTIFY trackingDetailsChanged FINAL)
	//! When the app was last seen running while the restored tracker kept running (it was closed or
	//! crashed meanwhile); invalid otherwise. See resolveInterruption().
	Q_PROPERTY(QDateTime interruptedAt READ interruptedAt NOTIFY interruptedAtChanged FINAL)
public:
	//! A restored running tracker whose app was gone for longer than this asks whether to count the
	//! time in between (interruptedAt); shorter gaps (e.g. a quick restart) just count.
	static constexpr int INTERRUPTION_THRESHOLD_SECONDS = 120;

	/*!
	 * \brief TimeTracker
	 * \param parent
	 */
	explicit TimeTracker(QObject* parent = nullptr);
	~TimeTracker() override;

	QString trackingComment() const;
	void setTrackingComment(const QString& comment);
	int trackingActivityId() const;
	void setTrackingActivityId(int activityId);
	QDateTime interruptedAt() const;

	/*!
	 * \brief resolveInterruption Decides about the time the app was gone (interruptedAt): count it,
	 * or stop the tracker as it was when the app was last seen (it's paused then)
	 */
	Q_INVOKABLE void resolveInterruption(bool countTimeAway);

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
	 * \brief updateRecentEntry Changes a local entry that isn't linked to a Redmine time entry. A
	 * rejected entry is sent to Redmine again afterwards.
	 * \param localId The entry's "id" in recentEntries
	 * \param changes "hours" (decimal hours), "comments" and "spent_on" ("yyyy-MM-dd"); optionally
	 * "activity_id", "custom_fields" (changed ones, [{"id", "value"}]) and "submit" (send an
	 * unconfirmed entry to Redmine)
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
	 * \brief addRecentEntry Saves a time entry locally and sends it to Redmine (submitTimeEntry())
	 * \param entry "issueId", "hours", "comments", "date" ("yyyy-MM-dd" or a Date) and optionally
	 * "activityId" and "customFields" ([{"id", "value"}])
	 * \return The new entry's local id; -1 if it couldn't be saved
	 */
	int addRecentEntry(const QVariantMap& entry);

	/*!
	 * \brief submitPendingEntries Sends the entries not on Redmine yet (Database::Pending) via
	 * submitTimeEntry(), except those already being sent. Call when Redmine is reachable.
	 */
	void submitPendingEntries();

	/*!
	 * \brief submissionFailed Handles a failed submission (RedmineClient::timeEntryCreateFailed):
	 * an entry Redmine refused is marked rejected with the reason, others are sent again later
	 */
	void submissionFailed(int localId, int httpStatus, const QString& error);

	/*!
	 * \brief deleteRecentEntry Deletes a local entry that isn't linked to a Redmine time entry (for a
	 * linked one, delete the Redmine entry: RedmineClient::deleteTimeEntry())
	 * \return false if it couldn't be deleted, e.g. because it's being sent to Redmine right now
	 */
	bool deleteRecentEntry(int localId);

	/*!
	 * \brief removeDeletedTimeEntry Forgets a time entry deleted on Redmine: its local copies and
	 * its Search Issue result. Connected to RedmineClient::timeEntryDeleted.
	 */
	void removeDeletedTimeEntry(int redmineId);

	/*!
	 * \brief sendToRedmine Marks an entry pending and sends it, e.g. an unconfirmed one the user
	 * knows isn't on Redmine
	 */
	void sendToRedmine(int localId);

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
	 * \param localId The local entry's id (the requestTag of submitTimeEntry()); ignored if not positive
	 * \return Whether the entry was just added by the user (addRecentEntry()) rather than sent
	 * again in the background, e.g. to confirm it
	 */
	bool linkCreatedTimeEntry(const QJsonObject& timeEntry, int localId);

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
	void trackingDetailsChanged();
	void interruptedAtChanged();

	/*!
	 * \brief submitTimeEntry Asks to create a local entry on Redmine; connect it to
	 * RedmineClient::createTimeEntry() with localId as the requestTag, and that request's outcome to
	 * linkCreatedTimeEntry() and submissionFailed()
	 */
	void submitTimeEntry(int localId, int issueId, double hours, const QString& comments, const QString& spentOn,
		int activityId, const QVariantList& customFields);

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

	//! Saves the tracker to the database (or clears it there when not tracking).
	void saveTrackingState();
	//! Restores a tracker saved by a previous run.
	void restoreTrackingState();

	//! Emits submitTimeEntry() for an entry (as from Database::getTimeEntry()) not being sent yet.
	void submitEntry(const QJsonObject& entry);

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
	//! Local ids of entries being sent to Redmine, so none is sent twice at a time.
	QSet<int> mSubmitting;
	//! Local ids of entries the user just added, whose creation on Redmine is worth a message.
	QSet<int> mInteractiveSubmissions;
	QString mTrackingComment;
	int mTrackingActivityId = 0;
	QDateTime mInterruptedAt;
	//! Seconds since the saved tracker's last_seen was last updated.
	int mSecondsSinceTouch = 0;
};
}
