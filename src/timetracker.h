#pragma once

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

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
	bool isTracking() const { return mTimer.isActive() || mIsPaused; }

	/*!
	 * \brief isPaused
	 * \return
	 */
	bool isPaused() const { return mIsPaused; }

	/*!
	 * \brief currentIssueId
	 * \return
	 */
	int currentIssueId() const { return mCurrentIssueId; }

	/*!
	 * \brief elapsedTime
	 * \return
	 */
	QString elapsedTime() const;

	/*!
	 * \brief recentEntries
	 * \return
	 */
	QVariantList recentEntries() const { return mRecentEntries; }

	/*!
	 * \brief getElapsedHours
	 * \return
	 */
	Q_INVOKABLE double getElapsedHours() const;

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

	/*!
	 * \brief saveRecentEntries
	 */
	void saveRecentEntries();

	QTimer mTimer;
	int mCurrentIssueId;
	bool mIsPaused;
	QDateTime mStartTime;
	qint64 mTotalElapsedTime;
	QVariantList mRecentEntries;
};
