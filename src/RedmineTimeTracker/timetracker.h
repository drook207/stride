#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
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
};
}
