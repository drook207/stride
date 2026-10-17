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
	 * \brief stageRedmineUpdate Remembers changes sent to Redmine for one of its time entries, to be
	 * applied locally by applyRedmineUpdate() once Redmine confirms them, so the local data never
	 * shows changes Redmine rejected
	 * \param redmineId Redmine's id of the time entry
	 * \param changes As for updateRecentEntry(), plus an optional "activity_id"
	 */
	Q_INVOKABLE void stageRedmineUpdate(int redmineId, const QVariantMap& changes);

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
	 */
	void addRecentEntry(const QVariantMap& entry);

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
	 */
	void applyRedmineUpdate(int redmineId);

	/*!
	 * \brief linkCreatedTimeEntry Links the local entry a time entry just created on Redmine was
	 * made from to that time entry, so it can be edited on Redmine later. Connected to
	 * RedmineClient::timeEntryCreated.
	 */
	void linkCreatedTimeEntry(const QJsonObject& timeEntry);

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
	//! Changes sent to Redmine but not confirmed yet, by Redmine time entry id
	QHash<int, QVariantMap> mPendingRedmineUpdates;
};
}
