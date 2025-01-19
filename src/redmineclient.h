
#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include <qtclasshelpermacros.h>

/*!
 * \brief The RedmineClient class
 */
class RedmineClient : public QObject {
	Q_OBJECT
	QML_ELEMENT
	Q_DISABLE_COPY_MOVE(RedmineClient)
	Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
	Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
	Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
	/*!
	 * \brief RedmineClient
	 * \param parent
	 */
	explicit RedmineClient(QObject* parent = nullptr);

	~RedmineClient();

	/*!
	 * \brief baseUrl
	 * \return
	 */
	QString baseUrl() const;

	/*!
	 * \brief setBaseUrl
	 * \param url
	 */
	void setBaseUrl(const QString& url);

	/*!
	 * \brief apiKey
	 * \return
	 */
	QString apiKey() const;

	/*!
	 * \brief setApiKey
	 * \param key
	 */
	void setApiKey(const QString& key);

	/*!
	 * \brief loading
	 * \return
	 */
	bool loading() const;

public slots:
	// Create a new time entry
	/*!
	 * \brief createTimeEntry
	 * \param issueId
	 * \param hours
	 * \param comments
	 * \param spentOn
	 * \param activityId
	 */
	void createTimeEntry(int issueId, double hours, const QString& comments,
		const QDateTime& spentOn = QDateTime::currentDateTime(),
		int activityId = 9); // 9 is often "Development"

	// Update an existing time entry
	/*!
	 * \brief updateTimeEntry
	 * \param timeEntryId
	 * \param hours
	 * \param comments
	 */
	void updateTimeEntry(int timeEntryId, double hours, const QString& comments);

	// Get time entries for an issue
	/*!
	 * \brief getTimeEntries
	 * \param issueId
	 */
	void getTimeEntries(int issueId);

signals:
	/*!
	 * \brief baseUrlChanged
	 */
	void baseUrlChanged();

	/*!
	 * \brief apiKeyChanged
	 */
	void apiKeyChanged();

	/*!
	 * \brief loadingChanged
	 */
	void loadingChanged();

	/*!
	 * \brief timeEntryCreated
	 * \param timeEntry
	 */
	void timeEntryCreated(const QJsonObject& timeEntry);

	/*!
	 * \brief timeEntryUpdated
	 * \param timeEntryId
	 */
	void timeEntryUpdated(int timeEntryId);

	/*!
	 * \brief timeEntriesReceived
	 * \param entries
	 */
	void timeEntriesReceived(const QJsonArray& entries);

	/*!
	 * \brief errorOccurred
	 * \param error
	 */
	void errorOccurred(const QString& error);

private slots:
	/*!
	 * \brief handleNetworkReply
	 * \param reply
	 */
	void handleNetworkReply(QNetworkReply* reply);

private:
	/*!
	 * \brief setLoading
	 * \param loading
	 */
	void setLoading(bool loading);

	/*!
	 * \brief createRequest
	 * \param endpoint
	 * \return
	 */
	QNetworkRequest createRequest(const QString& endpoint);
	QString mBaseUrl;
	QString mApiKey;
	bool mLoading;
	QNetworkAccessManager* mNetworkManager;
};
